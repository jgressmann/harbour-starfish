/* The MIT License (MIT)
 *
 * Copyright (c) 2018 Jean Gressmann <jean@0x42.de>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include "ScVodDataManagerWorker.h"
#include "ScVodDataManagerState.h"
#include "ScStopwatch.h"
#include "Sc.h"

#include <vodman/VMVodFileDownload.h>
#include <QSqlError>
#include <QSqlQuery>
#include <QNetworkAccessManager>
#include <QNetworkReply>


namespace
{
bool selectIdFromVodsWhereUrlShareIdEquals(QSqlQuery& q, qint64 urlShareId) {
    static const QString s_SelectIdFromVodsWhereVodUrlShareId = QStringLiteral("SELECT id FROM vods WHERE vod_url_share_id=?");
    if (!q.prepare(s_SelectIdFromVodsWhereVodUrlShareId)) {
        qCritical() << "failed to prepare query" << q.lastError();
        return false;
    }

    q.addBindValue(urlShareId);

    if (!q.exec()) {
        qCritical() << "failed to exec query" << q.lastError();
        return false;
    }

    return true;
}

} // anon

ScVodDataManagerWorker::~ScVodDataManagerWorker()
{
    qDebug() << "dtor entry";

    if (m_Vodman) {
        delete m_Vodman;
        m_Vodman = nullptr;
        qDebug() << "deleted vodman";
    }

    foreach (auto& key, m_ThumbnailRequests.keys()) {
        key->abort();
    }
    qDebug() << "aborted thumbnail requests";

    while (true) {
        {
            if (m_ThumbnailRequests.isEmpty())
            {
                break;
            }
        }

        QThread::msleep(100);
        qDebug() << "wait for requests";
        qDebug() << "thumbnail reqs gone" << (m_ThumbnailRequests.isEmpty());
    }

    m_Database = QSqlDatabase(); // to remove ref count
    QSqlDatabase::removeDatabase(QStringLiteral("ScVodDataManagerWorker"));

    qDebug() << "dtor exit";
}

ScVodDataManagerWorker::ScVodDataManagerWorker(const QSharedPointer<ScVodDataManagerState>& state)
    : m_SharedState(state)
    , m_Vodman(nullptr)
    , m_Manager(nullptr)
{
    // needed else: QObject::connect: Cannot queue arguments of type
    // at runtime
    qRegisterMetaType<VMVod>();

    m_Manager = new QNetworkAccessManager(this);
    connect(m_Manager, &QNetworkAccessManager::finished, this,
            &ScVodDataManagerWorker::requestFinished);


    m_Vodman = new ScVodman(this);
    connect(m_Vodman, &ScVodman::metaDataDownloadCompleted,
            this, &ScVodDataManagerWorker::onMetaDataDownloadCompleted);
    connect(m_Vodman, &ScVodman::fileDownloadChanged,
            this, &ScVodDataManagerWorker::onFileDownloadChanged);
    connect(m_Vodman, &ScVodman::fileDownloadCompleted,
            this, &ScVodDataManagerWorker::onFileDownloadCompleted);
    connect(m_Vodman, &ScVodman::downloadFailed,
            this, &ScVodDataManagerWorker::onDownloadFailed);

    m_Database = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), QStringLiteral("ScVodDataManagerWorker"));
    m_Database.setDatabaseName(m_SharedState->DatabaseFilePath);
    if (!m_Database.open()) {
        qCritical("Could not open database '%s'. Error: %s\n", qPrintable(m_SharedState->DatabaseFilePath), qPrintable(m_Database.lastError().text()));
        return;
    }
}

int ScVodDataManagerWorker::enqueue(Task&& req)
{
    return m_Queue.enqueue(std::move(req));
}

bool ScVodDataManagerWorker::cancel(int ticket)
{
    if (ticket <= -1) {
        qWarning() << "invalid ticket" << ticket;
        return false;
    }

    bool cancelled = false;
    auto result = m_Queue.cancel(ticket, &cancelled);
    assert(result);
    (void)result;

    return cancelled;
}

void ScVodDataManagerWorker::process()
{
    Task t;
    if (m_Queue.dequeue(t)) {
        t();
    }
}

void
ScVodDataManagerWorker::fetchMetaData(qint64 rowid, bool download) {
    ScStopwatch sw("fetchMetaData", 10);
    QSqlQuery q(m_Database);

    static const QString sql = QStringLiteral(
                "SELECT u.url, u.id FROM vods AS v INNER JOIN vod_url_share "
                "AS u ON v.vod_url_share_id=u.id WHERE v.id=?");
    if (!q.prepare(sql)) {
        qCritical() << "failed to prepare query" << q.lastError();
        return;
    }

    q.addBindValue(rowid);

    if (!q.exec()) {
        qCritical() << "failed to exec query" << q.lastError();
        return;
    }

    if (!q.next()) {
        qDebug() << "rowid" << rowid << "gone";
        return;
    }

    const auto vodUrl = q.value(0).toString();
    const auto urlShareId = qvariant_cast<qint64>(q.value(1));

    const auto metaDataFilePath = m_SharedState->m_MetaDataDir + QString::number(urlShareId);
    QFileInfo fi(metaDataFilePath);
    if (fi.exists()) {
        QFile file(metaDataFilePath);
        if (fi.created().addSecs(3600) >= QDateTime::currentDateTime()) {
            if (file.open(QIODevice::ReadOnly)) {
                VMVod vod;
                {
                    QDataStream s(&file);
                    s >> vod;
                }

                if (vod.isValid()) {
                    sw.stop();
                    emit metaDataAvailable(rowid, vod);
                } else {
                    // read invalid vod, try again
                    file.close();
                    file.remove();
                    if (download) {
                        emit fetchingMetaData(rowid);
                        fetchMetaData(urlShareId, vodUrl);
                    }
                }
            } else {
                file.remove();
                if (download) {
                    emit fetchingMetaData(rowid);
                    fetchMetaData(urlShareId, vodUrl);
                }
            }
        } else {
            // vod links probably expired
            file.remove();
            if (download) {
                emit fetchingMetaData(rowid);
                fetchMetaData(urlShareId, vodUrl);
            }
        }
    } else {
        if (download) {
            emit fetchingMetaData(rowid);
            fetchMetaData(urlShareId, vodUrl);
        }
    }
}

void
ScVodDataManagerWorker::fetchThumbnail(qint64 rowid, bool download)
{
    ScStopwatch sw("fetchThumbnail", 50);

    QSqlQuery q(m_Database);

    static const QString sql = QStringLiteral(
                "SELECT v.thumbnail_id, u.url, u.id FROM vods AS v INNER JOIN vod_url_share "
                "AS u ON v.vod_url_share_id=u.id WHERE v.id=?");
    if (!q.prepare(sql)) {
        qCritical() << "failed to prepare query" << q.lastError();
        return;
    }

    q.addBindValue(rowid);

    if (!q.exec()) {
        qCritical() << "failed to exec query" << q.lastError();
        return;
    }

    if (!q.next()) {
        qDebug() << "rowid" << rowid << "gone";
        return;
    }

    auto thumbnailId = q.value(0).isNull() ? 0 : qvariant_cast<qint64>(q.value(0));
    const auto vodUrl = q.value(1).toString();
    const auto vodUrlShareId = qvariant_cast<qint64>(q.value(2));
    if (thumbnailId) {
        // entry in table
        static const QString sql = QStringLiteral("SELECT file_name, url FROM vod_thumbnails WHERE id=?");
        if (!q.prepare(sql)) {
            qCritical() << "failed to prepare query" << q.lastError();
            return;
        }

        q.addBindValue(thumbnailId);

        if (!q.exec()) {
            qCritical() << "failed to exec query" << q.lastError();
            return;
        }

        if (!q.next()) {
            qDebug() << "ouch not data for thumbnail id" << thumbnailId;
            return;
        }

        auto fileName = q.value(0).toString();
        auto thumbNailFilePath = m_SharedState->m_ThumbnailDir + fileName;
        if (QFileInfo::exists(thumbNailFilePath)) {
            sw.stop();
            emit thumbnailAvailable(rowid, thumbNailFilePath);
        } else {
            if (download) {
                emit fetchingThumbnail(rowid);
                auto thumbnailUrl = q.value(1).toString();
                fetchThumbnailFromUrl(thumbnailId, thumbnailUrl);
            }
        }
    } else { // thumb nail id not set
        if (download) {
            auto metaDataFilePath = m_SharedState->m_MetaDataDir + QString::number(vodUrlShareId);
            if (QFileInfo::exists(metaDataFilePath)) {
                QFile file(metaDataFilePath);
                if (file.open(QIODevice::ReadOnly)) {
                    VMVod vod;
                    {
                        QDataStream s(&file);
                        s >> vod;
                    }

                    if (vod.isValid()) {
                        auto thumnailUrl = vod.description().thumbnailUrl();
                        if (thumnailUrl.isEmpty()) {
                            return;
                        }

                        static const QString sql = QStringLiteral("SELECT id FROM vod_thumbnails WHERE url=?");
                        if (!q.prepare(sql)) {
                            qCritical() << "failed to prepare query" << q.lastError();
                            return;
                        }

                        q.addBindValue(thumnailUrl);

                        if (!q.exec()) {
                            qCritical() << "failed to exec query" << q.lastError();
                            return;
                        }

                        if (q.next()) { // url has been retrieved, set thumbnail id in vods
                            thumbnailId = qvariant_cast<qint64>(q.value(0));

                            static const QString sql = QStringLiteral("UPDATE vods SET thumbnail_id=? WHERE id=?");
                            if (!q.prepare(sql)) {
                                qCritical() << "failed to prepare query" << q.lastError();
                                return;
                            }

                            q.addBindValue(thumbnailId);
                            q.addBindValue(rowid);

                            if (!q.exec()) {
                                qCritical() << "failed to exec query" << q.lastError();
                                return;
                            }
                        } else {
                            auto lastDot = thumnailUrl.lastIndexOf(QChar('.'));
                            auto extension = thumnailUrl.mid(lastDot);
                            QTemporaryFile tempFile(m_SharedState->m_ThumbnailDir + QStringLiteral("XXXXXX") + extension);
                            tempFile.setAutoRemove(false);
                            if (tempFile.open()) {
                                auto thumbNailFilePath = tempFile.fileName();
                                auto tempFileName = QFileInfo(thumbNailFilePath).fileName();

                                // insert row for thumbnail
                                static const QString sql = QStringLiteral("INSERT INTO vod_thumbnails (url, file_name) VALUES (?, ?)");
                                if (!q.prepare(sql)) {
                                    qCritical() << "failed to prepare query" << q.lastError();
                                    return;
                                }

                                q.addBindValue(thumnailUrl);
                                q.addBindValue(tempFileName);

                                if (!q.exec()) {
                                    qCritical() << "failed to exec query" << q.lastError();
                                    return;
                                }

                                thumbnailId = qvariant_cast<qint64>(q.lastInsertId());

                                // update vod row
                                static const QString sql2 = QStringLiteral("UPDATE vods SET thumbnail_id=? WHERE id=?");
                                if (!q.prepare(sql2)) {
                                    qCritical() << "failed to prepare query" << q.lastError();
                                    return;
                                }

                                q.addBindValue(thumbnailId);
                                q.addBindValue(rowid);

                                if (!q.exec()) {
                                    qCritical() << "failed to exec query" << q.lastError();
                                    return;
                                }

                                emit fetchingThumbnail(rowid);
                                fetchThumbnailFromUrl(thumbnailId, thumnailUrl);
                            } else {
                                qWarning() << "failed to allocate temporary file for thumbnail";
                            }
                        }
                    } else {
                        // read invalid vod, try again
                        file.close();
                        file.remove();
                        emit fetchingMetaData(rowid);
                        fetchMetaData(vodUrlShareId, vodUrl);
                    }
                } else {
                    file.remove();
                    emit fetchingMetaData(rowid);
                    fetchMetaData(vodUrlShareId, vodUrl);
                }
            } else {
                emit fetchingMetaData(rowid);
                fetchMetaData(vodUrlShareId, vodUrl);
            }
        }
    }
}

void
ScVodDataManagerWorker::fetchVod(qint64 rowid, int formatIndex, bool implicitlyStarted)
{
    QSqlQuery q(m_Database);

    static const QString sql = QStringLiteral(
                "SELECT\n"
                "    file_name,\n"
                "    progress,\n"
                "    format,"
                "    url,\n"
                "    vod_url_share_id\n"
                "FROM offline_vods\n"
                "WHERE id=?\n"
                );
    if (!q.prepare(sql)) {
        qCritical() << "failed to prepare query" << q.lastError();
        return;
    }


    q.addBindValue(rowid);

    if (!q.exec()) {
        qCritical() << "failed to exec query" << q.lastError();
        return;
    }

    QString vodFilePath;
    QString vodUrl;
    qint64 urlShareId = -1;
    QString formatId;

    float progress = 0;
    if (q.next()) {
        vodFilePath = m_SharedState->m_VodDir + q.value(0).toString();
        progress = q.value(1).toFloat();
        formatId = q.value(2).toString();
        vodUrl = q.value(3).toString();
        urlShareId = qvariant_cast<qint64>(q.value(4));
    } else {
        static const QString sql = QStringLiteral(
                    "SELECT\n"
                    "   url,\n"
                    "   vod_url_share_id\n"
                    "FROM vods v\n"
                    "INNER JOIN vod_url_share u ON v.vod_url_share_id=u.id\n"
                    "WHERE v.id=?\n"
                    );
        if (!q.prepare(sql)) {
            qCritical() << "failed to prepare query" << q.lastError();
            return;
        }

        q.addBindValue(rowid);

        if (!q.exec()) {
            qCritical() << "failed to exec query" << q.lastError();
            return;
        }

        if (!q.next()) {
            // row gone
            return;
        }

        vodUrl = q.value(0).toString();
        urlShareId = qvariant_cast<qint64>(q.value(1));
    }

    for (auto& value : m_VodmanFileRequests) {
        if (value.vod_url_share_id == urlShareId) {
            if (value.implicitlyStarted && !implicitlyStarted) {
                value.implicitlyStarted = false;
            }
            return; // already underway
        }
    }

    auto metaDataFilePath = m_SharedState->m_MetaDataDir + QString::number(urlShareId);
    if (QFileInfo::exists(metaDataFilePath)) {
        QFile file(metaDataFilePath);
        if (file.open(QIODevice::ReadOnly)) {
            VMVod vod;
            {
                QDataStream s(&file);
                s >> vod;
            }

            if (vod.isValid()) {
                if (!formatId.isEmpty()) {
                    auto formats = vod._formats();
                    for (auto i = 0; i < formats.size(); ++i) {
                        const auto& format = formats[i];
                        if (format.id() == formatId) {
                            qDebug() << "using previous selected format at index" << i << "id" << formatId;
                            formatIndex = i;
                            break;
                        }
                    }
                }

                if (formatIndex >= vod._formats().size()) {
                    qWarning() << "invalid format index (>=), not fetching vod" << formatIndex;
                    return;
                }

                auto format = vod._formats()[formatIndex];

                if (vodFilePath.isEmpty()) {
                    QTemporaryFile tempFile(m_SharedState->m_VodDir + QStringLiteral("XXXXXX.") + format.fileExtension());
                    if (tempFile.open()) {
                        vodFilePath = tempFile.fileName();
                        auto tempFileName = QFileInfo(vodFilePath).fileName();
                        static const QString sql = QStringLiteral(
                                    "INSERT INTO vod_files (file_name, format, width, height, progress) "
                                    "VALUES (?, ?, ?, ?, ?)");
                        if (!q.prepare(sql)) {
                            qCritical() << "failed to prepare query" << q.lastError();
                            return;
                        }

                        q.addBindValue(tempFileName);
                        q.addBindValue(format.id());
                        q.addBindValue(format.width());
                        q.addBindValue(format.height());
                        q.addBindValue(0);

                        if (!q.exec()) {
                            qCritical() << "failed to exec query" << q.lastError();
                            return;
                        }

                        auto vodFileId = qvariant_cast<qint64>(q.lastInsertId());

                        static const QString sql2 = QStringLiteral("UPDATE vod_url_share SET vod_file_id=? WHERE id=?");
                        if (!q.prepare(sql2)) {
                            qCritical() << "failed to prepare query" << q.lastError();
                            return;
                        }

                        q.addBindValue(vodFileId);
                        q.addBindValue(urlShareId);

                        if (!q.exec()) {
                            qCritical() << "failed to exec query" << q.lastError();
                            return;
                        }

                        // all good, keep file
                        tempFile.setAutoRemove(false);
                    } else {
                        qWarning() << "failed to create temporary file for vod" << rowid;
                        return;
                    }
                } else {
                    QFileInfo fi(vodFilePath);
                    if (fi.exists()) {
                        if (progress >= 1) {
                            emit vodAvailable(
                                        rowid,
                                        vodFilePath,
                                        progress,
                                        fi.size(),
                                        format.width(),
                                        format.height(),
                                        formatId);
                            return;
                        }
                    }
                }

                VodmanFileRequest r;
                r.token = m_Vodman->newToken();
                r.vod_url_share_id = urlShareId;
                r.implicitlyStarted = implicitlyStarted;
                m_VodmanFileRequests.insert(r.token, r);
                m_Vodman->startFetchFile(r.token, vod, formatIndex, vodFilePath);
                emit vodDownloadsChanged();

            } else {
                // read invalid vod, try again
                file.close();
                file.remove();
                fetchMetaData(urlShareId, vodUrl);
            }
        } else {
            file.remove();
            fetchMetaData(urlShareId, vodUrl);
        }
    } else {
        fetchMetaData(urlShareId, vodUrl);
    }
}

void
ScVodDataManagerWorker::queryVodFiles(qint64 rowid)
{
    QSqlQuery q(m_Database);

    if (!q.prepare(QStringLiteral(
                       "SELECT\n"
                       "    file_name,\n"
                       "    progress,\n"
                       "    width,\n"
                       "    height,\n"
                       "    format\n"
                       "FROM offline_vods\n"
                       "WHERE id=?\n"
                       ))) {
        qCritical() << "failed to prepare query" << q.lastError();
        return;
    }


    q.addBindValue(rowid);

    if (!q.exec()) {
        qCritical() << "failed to exec query" << q.lastError();
        return;
    }

    QFileInfo fi;
    while (q.next()) {
        auto filePath = m_SharedState->m_VodDir + q.value(0).toString();

        fi.setFile(filePath);
        if (fi.exists()) {
            auto progress = q.value(1).toFloat();
            auto width = q.value(2).toInt();
            auto height = q.value(3).toInt();
            auto format = q.value(4).toString();

            emit vodAvailable(rowid, filePath, progress, fi.size(), width, height, format);
        }
    }
}


void ScVodDataManagerWorker::fetchMetaData(qint64 urlShareId, const QString& url) {
    for (auto& value : m_VodmanMetaDataRequests) {
        if (value.vod_url_share_id == urlShareId) {
            ++value.refCount;
            return; // already underway
        }
    }

    VodmanMetaDataRequest r;
    r.vod_url_share_id = urlShareId;
    r.refCount = 1;
    auto token = m_Vodman->newToken();
    m_VodmanMetaDataRequests.insert(token, r);
    m_Vodman->startFetchMetaData(token, url);
}


void ScVodDataManagerWorker::onMetaDataDownloadCompleted(qint64 token, const VMVod& vod) {
    auto it = m_VodmanMetaDataRequests.find(token);
    if (it != m_VodmanMetaDataRequests.end()) {
        VodmanMetaDataRequest r = it.value();
        m_VodmanMetaDataRequests.erase(it);

        if (vod.isValid()) {
            // save vod meta data
            auto metaDataFilePath = m_SharedState->m_MetaDataDir + QString::number(r.vod_url_share_id);
            QFile file(metaDataFilePath);
            if (file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
                {
                    QDataStream s(&file);
                    s << vod;
                }

                file.close(); // to flush data

                // send event so that the match page can try to get the thumbnails again
                {
                    QSqlQuery q(m_Database);

                    // update title, length from vod
                    {
                        static const QString sql = QStringLiteral(
                                    "UPDATE vod_url_share SET title=?, length=? WHERE id=?");
                        if (!q.prepare(sql)) {
                            qCritical() << "failed to prepare query" << q.lastError();
                            return;
                        }

                        q.addBindValue(vod.description().title());
                        q.addBindValue(vod.description().duration());
                        q.addBindValue(r.vod_url_share_id);

                        if (!q.exec()) {
                            qCritical() << "failed to exec query" << q.lastError();
                            return;
                        }
                    }

                    // emit metaDataAvailable
                    {
                        if (Q_UNLIKELY(!selectIdFromVodsWhereUrlShareIdEquals(q, r.vod_url_share_id))) {
                            return;
                        }

                        while (q.next()) {
                            auto rowid = qvariant_cast<qint64>(q.value(0));
                            qDebug() << "metaDataAvailable" << rowid;
                            emit metaDataAvailable(rowid, vod);
                        }
                    }
                }
            } else {
                // error
                qDebug() << "failed to open meta data file" << metaDataFilePath << "for writing";
            }
        } else {
            qDebug() << "token" << token << "invalid vod" << vod;
        }
    }
}

void ScVodDataManagerWorker::onFileDownloadCompleted(qint64 token, const VMVodFileDownload& download) {
    auto it = m_VodmanFileRequests.find(token);
    if (it != m_VodmanFileRequests.end()) {
        const VodmanFileRequest r = it.value();
        m_VodmanFileRequests.erase(it);
        emit vodDownloadsChanged();

        QSqlQuery q(m_Database);
        if (download.progress() >= 1 && download.error() == VMVodEnums::VM_ErrorNone) {
            updateVodDownloadStatus(r.vod_url_share_id, download);
        } else {

        }
    }
}

void ScVodDataManagerWorker::onFileDownloadChanged(qint64 token, const VMVodFileDownload& download) {
    auto it = m_VodmanFileRequests.find(token);
    if (it != m_VodmanFileRequests.end()) {
        const VodmanFileRequest& r = it.value();

        updateVodDownloadStatus(r.vod_url_share_id, download);
    }
}

void ScVodDataManagerWorker::updateVodDownloadStatus(
        qint64 urlShareId,
        const VMVodFileDownload& download) {
    QSqlQuery q(m_Database);

    static const QString UpdateSql = QStringLiteral(
                "UPDATE vod_files\n"
                "SET progress=?\n"
                "WHERE id IN\n"
                "   (SELECT vod_file_id from vod_url_share WHERE id=?)");

    if (!q.prepare(UpdateSql)) {
        qCritical() << "failed to prepare query" << q.lastError();
        return;
    }

    q.addBindValue(download.progress());
    q.addBindValue(urlShareId);

    if (!q.exec()) {
        qCritical() << "failed to exec query" << q.lastError();
        return;
    }

    if (Q_UNLIKELY(!selectIdFromVodsWhereUrlShareIdEquals(q, urlShareId))) {
        return;
    }

    while (q.next()) {
        auto vodId = qvariant_cast<qint64>(q.value(0));
        emit vodAvailable(
                    vodId,
                    download.filePath(),
                    download.progress(),
                    download.fileSize(),
                    download.format().width(),
                    download.format().height(),
                    download.format().id());
    }
}

void
ScVodDataManagerWorker::onDownloadFailed(qint64 token, int serviceErrorCode) {

    qint64 urlShareId = -1;
    auto metaData = false;

    auto it = m_VodmanFileRequests.find(token);
    if (it != m_VodmanFileRequests.end()) {
        VodmanFileRequest r = it.value();
        m_VodmanFileRequests.erase(it);
        emit vodDownloadsChanged();

        urlShareId = r.vod_url_share_id;
        metaData = false;
    } else {
        auto it2 = m_VodmanMetaDataRequests.find(token);
        if (it2 != m_VodmanMetaDataRequests.end()) {
            VodmanMetaDataRequest r = it2.value();
            m_VodmanMetaDataRequests.erase(it2);

            urlShareId = r.vod_url_share_id;
            metaData = true;
        }
    }

    if (urlShareId >= 0) {
        QSqlQuery q(m_Database);
        if (Q_UNLIKELY(!selectIdFromVodsWhereUrlShareIdEquals(q, urlShareId))) {
            return;
        }

        while (q.next()) {
            auto rowid = qvariant_cast<qint64>(q.value(0));
            if (metaData) {
                emit metaDataDownloadFailed(rowid, serviceErrorCode);
            } else {
                emit vodDownloadFailed(rowid, serviceErrorCode);
            }
        }
    }
}


void
ScVodDataManagerWorker::cancelFetchVod(qint64 rowid) {
    QSqlQuery q(m_Database);

    static const QString selectUrlShareId = QStringLiteral("SELECT vod_url_share_id FROM vods WHERE id=?");
    if (!q.prepare(selectUrlShareId)) {
        qCritical() << "failed to prepare query" << q.lastError();
        return;
    }


    q.addBindValue(rowid);

    if (!q.exec()) {
        qCritical() << "failed to exec query" << q.lastError();
        return;
    }

    while (q.next()) {
        auto vodUrlShareId = qvariant_cast<qint64>(q.value(0));
        auto beg = m_VodmanFileRequests.begin();
        auto end = m_VodmanFileRequests.end();
        for (auto it = beg; it != end; ) {
            if (it.value().vod_url_share_id == vodUrlShareId) {
                m_Vodman->cancel(it.key());
                m_VodmanFileRequests.erase(it++);
                qDebug() << "cancel vod file request for url share id" << vodUrlShareId;
            } else {
                ++it;
            }
        }
    }

    emit vodDownloadsChanged();

    if (!q.prepare(QStringLiteral("SELECT id FROM vods WHERE vod_url_share_id in (%1)").arg(selectUrlShareId))) {
        qCritical() << "failed to prepare query" << q.lastError();
        return;
    }


    q.addBindValue(rowid);

    if (!q.exec()) {
        qCritical() << "failed to exec query" << q.lastError();
        return;
    }

    while (q.next()) {
        auto vodId = qvariant_cast<qint64>(q.value(0));
        emit vodDownloadCanceled(vodId);
    }
}

void
ScVodDataManagerWorker::cancelFetchMetaData(qint64 rowid) {
    QSqlQuery q(m_Database);

    static const QString sql = QStringLiteral(
                "SELECT vod_url_share_id FROM vods WHERE id=?");

    if (!q.prepare(sql)) {
        qCritical() << "failed to prepare query" << q.lastError();
        return;
    }

    q.addBindValue(rowid);

    if (!q.exec()) {
        qCritical() << "failed to exec query" << q.lastError();
        return;
    }

    if (!q.next()) {
        qDebug() << "rowid" << rowid << "gone";
        return;
    }

    const auto urlShareId = qvariant_cast<qint64>(q.value(0));

    auto beg = m_VodmanMetaDataRequests.begin();
    auto end = m_VodmanMetaDataRequests.end();
    for (auto it = beg; it != end; ++it) {
        if (it.value().vod_url_share_id == urlShareId) {
            qDebug() << "canceled meta data fetch for" << rowid;
            if (--it.value().refCount == 0) {
                auto token = it.key();
                m_VodmanMetaDataRequests.remove(token);
                m_Vodman->cancel(token);
            }
            return;
        }
    }
}


void
ScVodDataManagerWorker::cancelFetchThumbnail(qint64 rowid) {


    auto beg = m_ThumbnailRequests.begin();
    auto end = m_ThumbnailRequests.end();
    for (auto it = beg; it != end; ++it) {
        if (it.value().rowid == rowid) {
            qDebug() << "cancel thumbnail fetch for" << rowid;
            auto reply = it.key();
            m_ThumbnailRequests.remove(reply);
            reply->abort();
            return;
        }
    }
}



void
ScVodDataManagerWorker::requestFinished(QNetworkReply* reply) {
    ScStopwatch sw("requestFinished");
    reply->deleteLater();

    auto it = m_ThumbnailRequests.find(reply);
    if (it != m_ThumbnailRequests.end()) {
        auto r = it.value();
        m_ThumbnailRequests.erase(it);
        thumbnailRequestFinished(reply, r);
    }
}


void
ScVodDataManagerWorker::thumbnailRequestFinished(QNetworkReply* reply, ThumbnailRequest& r) {
    switch (reply->error()) {
    case QNetworkReply::OperationCanceledError:
        break;
    case QNetworkReply::NoError: {
        // Get the http status code
        int v = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        if (v >= 200 && v < 300) {// Success

            // Here we got the final reply
            auto bytes = reply->readAll();
            addThumbnail(r.rowid, bytes);

        }
        else if (v >= 300 && v < 400) {// Redirection

            // Get the redirection url
            QUrl newUrl = reply->attribute(QNetworkRequest::RedirectionTargetAttribute).toUrl();
            // Because the redirection url can be relative,
            // we have to use the previous one to resolve it
            newUrl = reply->url().resolved(newUrl);

            m_ThumbnailRequests.insert(m_Manager->get(Sc::makeRequest(newUrl)), r);
        } else  {
            qDebug() << "Http status code:" << v;
        }
    } break;
    default: {
        qDebug() << "Network request failed: " << reply->errorString() << reply->url();

        QSqlQuery q(m_Database);
        if (!q.prepare(QStringLiteral("SELECT id FROM vods WHERE thumbnail_id=?"))) {
            qCritical() << "failed to prepare query" << q.lastError();
            return;
        }

        q.addBindValue(r.rowid);

        if (!q.exec()) {
            qCritical() << "failed to exec query" << q.lastError();
            return;
        }

        auto url = reply->url().toString();
        while (q.next()) {
            auto vodRowId = qvariant_cast<qint64>(q.value(0));
            emit thumbnailDownloadFailed(vodRowId, reply->error(), url);
        }
    } break;
    }
}


void ScVodDataManagerWorker::fetchThumbnailFromUrl(qint64 rowid, const QString& url) {
    foreach (const auto& value, m_ThumbnailRequests.values()) {
        if (value.rowid == rowid) {
            return; // already underway
        }
    }

    ThumbnailRequest r;
    r.rowid = rowid;
    m_ThumbnailRequests.insert(m_Manager->get(Sc::makeRequest(url)), r);
}


void
ScVodDataManagerWorker::addThumbnail(qint64 rowid, const QByteArray& bytes) {

    QSqlQuery q(m_Database);

    if (!q.prepare(QStringLiteral("SELECT file_name FROM vod_thumbnails WHERE id=?"))) {
        qCritical() << "failed to prepare query" << q.lastError();
        return;
    }

    q.addBindValue(rowid);

    if (!q.exec()) {
        qCritical() << "failed to exec query" << q.lastError();
        return;
    }

    if (!q.next()) {
        qDebug() << "thumbnail rowid" << rowid << "gone";
        return;
    }

    auto thumbnailFileName = q.value(0).toString();

    // this nonsense is necessary b/c QImage doesn't use the
    // mime type but the extension
    auto lastDot = thumbnailFileName.lastIndexOf(QChar('.'));
    auto extension = thumbnailFileName.mid(lastDot + 1);
    auto mimeData = QMimeDatabase().mimeTypeForData(bytes);
    if (mimeData.isValid() && !mimeData.suffixes().contains(extension)) {
        auto newFileName = thumbnailFileName.left(lastDot) + QStringLiteral(".") + mimeData.preferredSuffix();
        qDebug() << "changing" << thumbnailFileName << "to" << newFileName;
        if (!q.prepare(QStringLiteral("UPDATE vod_thumbnails SET file_name=? WHERE id=?"))) {
            qCritical() << "failed to prepare query" << q.lastError();
            return;
        }

        q.addBindValue(newFileName);
        q.addBindValue(rowid);

        if (!q.exec()) {
            qCritical() << "failed to exec query" << q.lastError();
            return;
        }

        // remove old thumbnail file if it exists
        QFile::remove(m_SharedState->m_ThumbnailDir + thumbnailFileName);

        thumbnailFileName = newFileName;
    }

    auto thumbnailFilePath = m_SharedState->m_ThumbnailDir + thumbnailFileName;
    QFile file(thumbnailFilePath);
    if (file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        if (bytes.size() == file.write(bytes)) {
            file.close();
            // all is well


            // notify thumbnail is available
            if (!q.prepare(QStringLiteral("SELECT id FROM vods WHERE thumbnail_id=?"))) {
                qCritical() << "failed to prepare query" << q.lastError();
                return;
            }

            q.addBindValue(rowid);

            if (!q.exec()) {
                qCritical() << "failed to exec query" << q.lastError();
                return;
            }

            while (q.next()) {
                auto vodRowId = qvariant_cast<qint64>(q.value(0));
                qDebug() << "thumbnailAvailable" << vodRowId << thumbnailFilePath;
                emit thumbnailAvailable(vodRowId, thumbnailFilePath);
            }
        } else {
            // fix me, no space left
            qDebug() << "no space left on device";
        }
    } else {
        // fix me
        qDebug() << "failed to open" << thumbnailFilePath << "for writing";
    }
}

void ScVodDataManagerWorker::maxConcurrentMetaDataDownloadsChanged(int value)
{
    m_Vodman->setMaxConcurrentMetaDataDownloads(value);
}
