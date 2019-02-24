/* The MIT License (MIT)
 *
 * Copyright (c) 2018, 2019 Jean Gressmann <jean@0x42.de>
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
#include "Sc.h"
#include "ScDatabaseStoreQueue.h"
#include "VMVodFileDownload.h"
#include <QSqlError>
#include <QSqlQuery>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QFile>
#include <QFileInfo>
#include <QDataStream>
#include <QTemporaryFile>
#include <QMimeDatabase>


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

bool selectUrlShareIdFromVodsWhereIdEquals(QSqlQuery& q, qint64 vodId) {
    static const QString sql = QStringLiteral("SELECT vod_url_share_id FROM vods WHERE id=?");
    if (!q.prepare(sql)) {
        qCritical() << "failed to prepare query" << q.lastError();
        return false;
    }

    q.addBindValue(vodId);

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

    qDebug() << "disconnect";
    disconnect();

    delete m_Vodman;
    qDebug() << "deleted vodman";

    foreach (auto key, m_ThumbnailRequests.keys()) {
        key->abort();
        delete key;
    }
    qDebug() << "aborted thumbnail requests";

    delete m_Manager;
    qDebug() << "deleted network access manager";

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
    qRegisterMetaType<VMVod>("VMVod");
    qRegisterMetaType<ScVodIdList>("ScVodIdList");

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

    // database store queue->this
    connect(m_SharedState->DatabaseStoreQueue.data(), &ScDatabaseStoreQueue::completed,
            this, &ScVodDataManagerWorker::databaseStoreCompleted);
    // this->database store queue
    connect(this, &ScVodDataManagerWorker::startProcessDatabaseStoreQueue,
            m_SharedState->DatabaseStoreQueue.data(), &ScDatabaseStoreQueue::process);
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
ScVodDataManagerWorker::fetchMetaData(qint64 urlShareId, const QString& url, bool download)
{
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
                    emit metaDataAvailable(urlShareId, vod);
                } else {
                    // read invalid vod, try again
                    file.close();
                    file.remove();
                    if (download) {
                        emit fetchingMetaData(urlShareId);
                        fetchMetaData(urlShareId, url);
                    } else {
                        emit metaDataUnavailable(urlShareId);
                    }
                }
            } else {
                file.remove();
                if (download) {
                    emit fetchingMetaData(urlShareId);
                    fetchMetaData(urlShareId, url);
                } else {
                    emit metaDataUnavailable(urlShareId);
                }
            }
        } else {
            // vod links probably expired
            file.remove();
            if (download) {
                emit fetchingMetaData(urlShareId);
                fetchMetaData(urlShareId, url);
            } else {
                emit metaDataUnavailable(urlShareId);
            }
        }
    } else {
        if (download) {
            emit fetchingMetaData(urlShareId);
            fetchMetaData(urlShareId, url);
        } else {
            emit metaDataUnavailable(urlShareId);
        }
    }
}

void
ScVodDataManagerWorker::fetchThumbnail(qint64 urlShareId, bool download)
{
    QSqlQuery q(m_Database);

    static const QString sql = QStringLiteral(
                "SELECT vod_thumbnail_id, url FROM vod_url_share WHERE id=?");
    if (!q.prepare(sql)) {
        qCritical() << "failed to prepare query" << q.lastError();
        return;
    }

    q.addBindValue(urlShareId);

    if (!q.exec()) {
        qCritical() << "failed to exec query" << q.lastError();
        return;
    }

    if (!q.next()) {
        qDebug() << "url share id" << urlShareId << "gone";
        return;
    }

    auto thumbnailId = q.value(0).isNull() ? 0 : qvariant_cast<qint64>(q.value(0));
    const auto vodUrl = q.value(1).toString();
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
            qDebug() << "ouch not data for thumbnail id" << thumbnailId << "url share id" << urlShareId;
            return;
        }

        auto fileName = q.value(0).toString();
        auto thumbNailFilePath = m_SharedState->m_ThumbnailDir + fileName;
        auto hasThumbnailFile = false;
        QFileInfo fi(thumbNailFilePath);
        if (fi.exists()) {
            if (fi.size() > 0) {
                hasThumbnailFile = true;
            } else {
                // keep empty file so that temp file allocation for
                // other thumbnails doesn't allocate same name multiple times
            }
        }

        if (hasThumbnailFile) {
            emit thumbnailAvailable(urlShareId, thumbNailFilePath);
        } else {
            auto thumbnailUrl = q.value(1).toString();
            if (download) {
                emit fetchingThumbnail(urlShareId);
                fetchThumbnailFromUrl(urlShareId, thumbnailId, thumbnailUrl);
            } else {
                emit thumbnailUnavailable(urlShareId, TE_Other);
            }
        }
    } else { // thumbnail id not set
        if (download) {
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
                        auto thumnailUrl = vod.description().thumbnailUrl();
                        if (thumnailUrl.isEmpty()) {
                            emit thumbnailUnavailable(urlShareId, TE_Other);
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

                        static const QString UpdateSql = QStringLiteral("UPDATE vod_url_share SET vod_thumbnail_id=? WHERE id=?");

                        if (q.next()) { // url has been retrieved, set thumbnail id in vods
                            thumbnailId = qvariant_cast<qint64>(q.value(0));
                            auto tid = m_SharedState->DatabaseStoreQueue->newTransactionId();
                            DatabaseCallback postUpdate = [=](qint64 _thumbnailId, bool error)
                            {
                                Q_UNUSED(_thumbnailId);
                                if (error) {
                                    emit thumbnailUnavailable(urlShareId, TE_Other);
                                } else {
                                    emit fetchingThumbnail(urlShareId);
                                    fetchThumbnailFromUrl(urlShareId, thumbnailId, thumnailUrl);
                                }
                            };
                            m_PendingDatabaseStores.insert(tid, postUpdate);
                            emit startProcessDatabaseStoreQueue(tid, UpdateSql, { thumbnailId, urlShareId });
                            emit startProcessDatabaseStoreQueue(tid, {}, {});
                        } else {
                            auto lastDot = thumnailUrl.lastIndexOf(QChar('.'));
                            auto extension = thumnailUrl.mid(lastDot);
                            QTemporaryFile tempFile(m_SharedState->m_ThumbnailDir + QStringLiteral("XXXXXX") + extension);
                            tempFile.setAutoRemove(false);
                            if (tempFile.open()) {
                                auto thumbNailFilePath = tempFile.fileName();
                                tempFile.close();
                                auto tempFileName = QFileInfo(thumbNailFilePath).fileName();



                                // insert row for thumbnail
                                static const QString InsertSql = QStringLiteral("INSERT INTO vod_thumbnails (url, file_name) VALUES (?, ?)");
                                auto tid = m_SharedState->DatabaseStoreQueue->newTransactionId();
                                DatabaseCallback postInsert = [=](qint64 thumbnailId, bool error)
                                {
                                    if (error) {
                                        QFile::remove(thumbNailFilePath);
                                        emit thumbnailUnavailable(urlShareId, TE_Other);
                                    } else {
                                        auto tid = m_SharedState->DatabaseStoreQueue->newTransactionId();
                                        emit startProcessDatabaseStoreQueue(tid, UpdateSql, { thumbnailId, urlShareId });
                                        emit startProcessDatabaseStoreQueue(tid, {}, {});
                                        emit fetchingThumbnail(urlShareId);
                                        fetchThumbnailFromUrl(urlShareId, thumbnailId, thumnailUrl);
                                    }
                                };
                                m_PendingDatabaseStores.insert(tid, postInsert);

                                emit startProcessDatabaseStoreQueue(tid, InsertSql, { thumnailUrl, tempFileName });
                                emit startProcessDatabaseStoreQueue(tid, {}, {});
                            } else {
                                qWarning() << "failed to allocate temporary file for thumbnail";
                            }
                        }
                    } else {
                        // read invalid vod, try again
//                        file.close();
//                        file.remove();
                        // don't try to help, it will mess up meta data fetch logic
                        emit thumbnailUnavailable(urlShareId, TE_MetaDataUnavailable);
//                        emit fetchingMetaData(urlShareId);
//                        fetchMetaData(urlShareId, vodUrl);
                    }
                } else {
                    // don't try to help, it will mess up meta data fetch logic
//                    file.remove();
//                    emit metaDataUnavailable(urlShareId);
                    emit thumbnailUnavailable(urlShareId, TE_MetaDataUnavailable);
                }
            } else {
                // don't try to help, it will mess up meta data fetch logic
//                emit metaDataUnavailable(urlShareId);
                emit thumbnailUnavailable(urlShareId, TE_MetaDataUnavailable);
            }
        } else {
            emit thumbnailUnavailable(urlShareId, TE_MetaDataUnavailable);
        }
    }
}

void
ScVodDataManagerWorker::fetchVod(qint64 urlShareId, int formatIndex)
{
    for (auto& value : m_VodmanFileRequests) {
        if (value.vod_url_share_id == urlShareId) {
            ++value.refCount;
            return; // already underway
        }
    }

    QSqlQuery q(m_Database);

    // step 1: is there an existing vod file?
    static const QString sql = QStringLiteral(
                "SELECT\n"
                "    file_name,\n"
                "    progress,\n"
                "    format,\n"
                "    width,\n"
                "    height\n"
                "FROM vod_url_share u\n"
                "LEFT OUTER JOIN vod_files f ON u.vod_file_id=f.id\n"
                "WHERE u.id=?\n"
                );

    if (!q.prepare(sql)) {
        qCritical() << "failed to prepare query" << q.lastError() << sql;
        return;
    }

    q.addBindValue(urlShareId);

    if (!q.exec()) {
        qCritical() << "failed to exec query" << q.lastError();
        return;
    }

    if (!q.next()) {
        qDebug() << "gone";
        return;
    }

    auto vodFileName = q.value(0).toString();
    auto progress = q.value(1).toFloat();
    auto formatId = q.value(2).toString();
    auto width = q.value(3).toInt();
    auto height = q.value(4).toInt();
    QString vodFilePath;

    if (!vodFileName.isEmpty()) {
        vodFilePath = m_SharedState->m_VodDir + vodFileName;
        QFileInfo fi(vodFilePath);
        if (fi.exists() && fi.size() > 0) {
            if (progress >= 1) {
                emit vodAvailable(
                            urlShareId,
                            vodFilePath,
                            progress,
                            fi.size(),
                            width,
                            height,
                            formatId);
            }
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
                    emit vodDownloadFailed(urlShareId, VMVodEnums::VM_ErrorFormatNotAvailable);
                    return;
                }

                auto format = vod._formats()[formatIndex];

                if (vodFilePath.isEmpty()) {
                    QTemporaryFile tempFile(m_SharedState->m_VodDir + QStringLiteral("XXXXXX.") + format.fileExtension());
                    tempFile.setAutoRemove(false);
                    if (tempFile.open()) {
                        vodFilePath = tempFile.fileName();
                        tempFile.close();

                        auto tempFileName = QFileInfo(vodFilePath).fileName();
                        static const QString InsertSql = QStringLiteral(
                                    "INSERT INTO vod_files (file_name, format, width, height, progress) "
                                    "VALUES (?, ?, ?, ?, ?)");
                        static const QString UpdateSql = QStringLiteral("UPDATE vod_url_share SET vod_file_id=? WHERE id=?");


                        auto tid = m_SharedState->DatabaseStoreQueue->newTransactionId();
                        DatabaseCallback postInsert = [=](qint64 vodFileId, bool error)
                        {
                            if (error) {
                                QFile::remove(vodFilePath);
                            } else {
                                auto tid = m_SharedState->DatabaseStoreQueue->newTransactionId();
                                emit startProcessDatabaseStoreQueue(tid, UpdateSql, { vodFileId, urlShareId });
                                emit startProcessDatabaseStoreQueue(tid, {}, {});
                            }
                        };
                        m_PendingDatabaseStores.insert(tid, postInsert);

                        emit startProcessDatabaseStoreQueue(tid, InsertSql, { tempFileName, format.id(), format.width(), format.height(), 0 });
                        emit startProcessDatabaseStoreQueue(tid, {}, {});

                        // all good, keep file

                    } else {
                        qWarning() << "failed to create temporary file for url share id" << urlShareId;
                        emit vodDownloadFailed(urlShareId, VMVodEnums::VM_ErrorAccess);
                        return;
                    }
                }

                VodmanFileRequest r;
                r.token = m_Vodman->newToken();
                r.vod_url_share_id = urlShareId;
                r.refCount = 1;
                m_VodmanFileRequests.insert(r.token, r);
                m_Vodman->startFetchFile(r.token, vod, formatIndex, vodFilePath);
                notifyVodDownloadsChanged();
            } else {
                // don't help, will mess up meta data fetch logic
                emit vodUnavailable(urlShareId);
            }
        } else {
            // don't help, will mess up meta data fetch logic
            emit vodUnavailable(urlShareId);
        }
    } else {
        // don't help, will mess up meta data fetch logic
        emit vodUnavailable(urlShareId);
    }
}

void
ScVodDataManagerWorker::queryVodFiles(qint64 urlShareId)
{
    QSqlQuery q(m_Database);

    if (!q.prepare(QStringLiteral(
                       "SELECT\n"
                       "    file_name,\n"
                       "    progress,\n"
                       "    width,\n"
                       "    height,\n"
                       "    format\n"
                       "FROM vod_url_share u\n"
                       "INNER JOIN vod_files f ON u.vod_file_id=f.id\n"
                       "WHERE u.id=?\n"
                       ))) {
        qCritical() << "failed to prepare query" << q.lastError();
        return;
    }


    q.addBindValue(urlShareId);

    if (!q.exec()) {
        qCritical() << "failed to exec query" << q.lastError();
        return;
    }

    QFileInfo fi;
    if (q.next()) {
        auto filePath = m_SharedState->m_VodDir + q.value(0).toString();

        fi.setFile(filePath);
        if (fi.exists()) {
            auto progress = q.value(1).toFloat();
            auto width = q.value(2).toInt();
            auto height = q.value(3).toInt();
            auto format = q.value(4).toString();

            emit vodAvailable(urlShareId, filePath, progress, fi.size(), width, height, format);
        } else {
            emit vodUnavailable(urlShareId);
        }
    } else {
        emit vodUnavailable(urlShareId);
    }
}


void
ScVodDataManagerWorker::fetchMetaData(qint64 urlShareId, const QString& url) {

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
                QDataStream s(&file);
                s << vod;
                if (s.status() == QDataStream::Ok && file.flush()) {
                    file.close();

                    // send event so that the match page can try to get the thumbnails again
                    auto vodUrlShareId = r.vod_url_share_id;

                    static const QString UpdateSql = QStringLiteral(
                                "UPDATE vod_url_share SET title=?, length=? WHERE id=?");
                    auto tid = m_SharedState->DatabaseStoreQueue->newTransactionId();
                    DatabaseCallback postUpdate = [=](qint64 vodFileId, bool error)
                    {
                        (void)vodFileId;
                        if (error) {
                            emit metaDataDownloadFailed(vodUrlShareId, VMVodEnums::VM_ErrorUnknown);
                        } else {
                            emit metaDataAvailable(vodUrlShareId, vod);
                        }
                    };
                    m_PendingDatabaseStores.insert(tid, postUpdate);
                    emit startProcessDatabaseStoreQueue(tid, UpdateSql, { vod.description().title(), vod.description().duration(), vodUrlShareId });
                    emit startProcessDatabaseStoreQueue(tid, {}, {});
                } else {
                    qDebug() << "failed to write meta data file" << metaDataFilePath;
                    emit metaDataDownloadFailed(r.vod_url_share_id, VMVodEnums::VM_ErrorIo);
                }
            } else {
                // error
                qDebug() << "failed to open meta data file" << metaDataFilePath << "for writing";
                emit metaDataDownloadFailed(r.vod_url_share_id, VMVodEnums::VM_ErrorAccess);
            }
        } else {
            qDebug() << "token" << token << "invalid vod" << vod;
            emit metaDataDownloadFailed(r.vod_url_share_id, VMVodEnums::VM_ErrorNoVideo);
        }
    }
}

void ScVodDataManagerWorker::onFileDownloadCompleted(qint64 token, const VMVodFileDownload& download) {
    auto it = m_VodmanFileRequests.find(token);
    if (it != m_VodmanFileRequests.end()) {
        const VodmanFileRequest r = it.value();
        m_VodmanFileRequests.erase(it);

        emit vodAvailable(
                    r.vod_url_share_id,
                    download.filePath(),
                    download.progress(),
                    download.fileSize(),
                    download.format().width(),
                    download.format().height(),
                    download.format().id());

        notifyVodDownloadsChanged();
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

    static const QString UpdateSql = QStringLiteral(
                "UPDATE vod_files\n"
                "SET progress=?\n"
                "WHERE id IN\n"
                "   (SELECT vod_file_id FROM vod_url_share WHERE id=?)");

    DatabaseCallback postUpdate = [=](qint64 insertId, bool error)
    {
        (void)insertId;
        if (!error) {
            // guard against reset to zero at restart of canceled download
            if (download.progress() > 0) {
                emit fetchingVod(
                            urlShareId,
                            download.filePath(),
                            download.progress(),
                            download.fileSize(),
                            download.format().width(),
                            download.format().height(),
                            download.format().id());
            }
        }
    };

    auto tid = m_SharedState->DatabaseStoreQueue->newTransactionId();
    m_PendingDatabaseStores.insert(tid, postUpdate);
    emit startProcessDatabaseStoreQueue(tid, UpdateSql, { download.progress(), urlShareId });
    emit startProcessDatabaseStoreQueue(tid, {}, {});
}

void
ScVodDataManagerWorker::onDownloadFailed(qint64 token, VMVodEnums::Error serviceErrorCode) {

    QSqlQuery q(m_Database);
    qint64 urlShareId = -1;
    auto metaData = false;

    auto it = m_VodmanFileRequests.find(token);
    if (it != m_VodmanFileRequests.end()) {
        VodmanFileRequest r = it.value();
        m_VodmanFileRequests.erase(it);

        urlShareId = r.vod_url_share_id;
        metaData = false;
        notifyVodDownloadsChanged();
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
        if (metaData) {
            emit metaDataDownloadFailed(urlShareId, serviceErrorCode);
        } else {
            emit vodDownloadFailed(urlShareId, serviceErrorCode);
        }
    }
}

void
ScVodDataManagerWorker::cancelFetchVod(qint64 urlShareId)
{
    auto canceled = false;
    auto beg = m_VodmanFileRequests.begin();
    auto end = m_VodmanFileRequests.end();
    for (auto it = beg; it != end; ++it) {
        if (it.value().vod_url_share_id == urlShareId) {
            if (--it.value().refCount == 0) {
                qDebug() << "cancel vod file download for url share id" << urlShareId;
                m_Vodman->cancel(it.key());
                m_VodmanFileRequests.erase(it);
                canceled = true;
                break;
            }
        }
    }

    if (canceled) {
        emit vodDownloadCanceled(urlShareId);
        notifyVodDownloadsChanged();
    }
}

void
ScVodDataManagerWorker::cancelFetchMetaData(qint64 urlShareId)
{
    auto beg = m_VodmanMetaDataRequests.begin();
    auto end = m_VodmanMetaDataRequests.end();
    for (auto it = beg; it != end; ++it) {
        if (it.value().vod_url_share_id == urlShareId) {
            if (--it.value().refCount == 0) {
                qDebug() << "cancel meta data file download for url share id" << urlShareId;
                auto token = it.key();
                m_VodmanMetaDataRequests.remove(token);
                m_Vodman->cancel(token);
            }
            return;
        }
    }
}


void
ScVodDataManagerWorker::cancelFetchThumbnail(qint64 urlShareId)
{
    const auto beg = m_ThumbnailRequests.begin();
    const auto end = m_ThumbnailRequests.end();
    for (auto it = beg; it != end; ++it) {
        if (it.value().url_share_id == urlShareId) {
            if (--it.value().refCount == 0) {
                qDebug() << "cancel thumbnail file download for url share id" << urlShareId;
                auto reply = it.key();
                m_ThumbnailRequests.remove(reply);
                reply->abort();
            }
            return;
        }
    }
}



void
ScVodDataManagerWorker::requestFinished(QNetworkReply* reply)
{
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
        if (v >= 200 && v < 300) { // Success
            // Here we got the final reply
            auto bytes = reply->readAll();
            if (bytes.isEmpty()) {
                // this really shouldn't happen anymore
                // the code remains here to safeguard against errors
                // in the program logic
                emit thumbnailDownloadFailed(r.url_share_id, reply->error(), reply->url().toString());
            } else {
                addThumbnail(r, bytes);
            }
        } else if (v >= 300 && v < 400) { // Redirection
            // Get the redirection url
            QUrl newUrl = reply->attribute(QNetworkRequest::RedirectionTargetAttribute).toUrl();
            // Because the redirection url can be relative,
            // we have to use the previous one to resolve it
            newUrl = reply->url().resolved(newUrl);

            m_ThumbnailRequests.insert(m_Manager->get(Sc::makeRequest(newUrl)), r);
        } else  {
            qDebug() << "Http status code:" << v;
            emit thumbnailDownloadFailed(r.url_share_id, reply->error(), reply->url().toString());
        }
    } break;
    case QNetworkReply::ContentNotFoundError:
    case QNetworkReply::ContentGoneError: {
        qDebug() << "Network request failed: " << reply->errorString() << reply->url();
        removeThumbnail(r);
    } break;
    default: {
        qDebug() << "Network request failed: " << reply->errorString() << reply->url();
        emit thumbnailDownloadFailed(r.url_share_id, reply->error(), reply->url().toString());
    } break;
    }
}

void ScVodDataManagerWorker::fetchThumbnailFromUrl(qint64 urlShareId, qint64 thumbnailId, const QString& url) {
    auto beg = m_ThumbnailRequests.begin();
    auto end = m_ThumbnailRequests.end();
    for (auto it = beg; it != end; ++it) {
        if (it.value().thumbnail_id == thumbnailId) {
            ++it.value().refCount;
            return; // already underway
        }
    }

    ThumbnailRequest r;
    r.refCount = 1;
    r.thumbnail_id = thumbnailId;
    r.url_share_id = urlShareId;
    m_ThumbnailRequests.insert(m_Manager->get(Sc::makeRequest(url)), r);
}


void
ScVodDataManagerWorker::addThumbnail(ThumbnailRequest& r, const QByteArray& bytes) {

    QSqlQuery q(m_Database);

    if (!q.prepare(QStringLiteral("SELECT file_name FROM vod_thumbnails WHERE id=?"))) {
        qCritical() << "failed to prepare query" << q.lastError();
        return;
    }

    q.addBindValue(r.thumbnail_id);

    if (!q.exec()) {
        qCritical() << "failed to exec query" << q.lastError();
        return;
    }

    if (!q.next()) {
        qDebug() << "thumbnail rowid" << r.thumbnail_id << "gone";
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
        qDebug() << "renaming" << thumbnailFileName << "to" << newFileName;


        auto tid = m_SharedState->DatabaseStoreQueue->newTransactionId();
        emit startProcessDatabaseStoreQueue(
                    tid,
                    QStringLiteral("UPDATE vod_thumbnails SET file_name=? WHERE id=?"),
                    { newFileName, r.thumbnail_id });
        emit startProcessDatabaseStoreQueue(tid, {}, {});

        // remove old thumbnail file if it exists
        QFile::remove(m_SharedState->m_ThumbnailDir + thumbnailFileName);

        thumbnailFileName = newFileName;
    }

    auto thumbnailFilePath = m_SharedState->m_ThumbnailDir + thumbnailFileName;
    QFile file(thumbnailFilePath);
    if (file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        if (bytes.size() == file.write(bytes) && file.flush()) {
            file.close();
            // all is well

//            qDebug() << "thumbnailAvailable" << r.url_share_id << thumbnailFilePath;
            emit thumbnailAvailable(r.url_share_id, thumbnailFilePath);
        } else {
            qDebug() << "no space left on device";
            emit thumbnailDownloadFailed(r.url_share_id, VMVodEnums::VM_ErrorNoSpaceLeftOnDevice, QString());
        }
    } else {
        qDebug() << "failed to open" << thumbnailFilePath << "for writing";
        emit thumbnailDownloadFailed(r.url_share_id, VMVodEnums::VM_ErrorAccess, QString());
    }
}


void
ScVodDataManagerWorker::removeThumbnail(ThumbnailRequest& r)
{
    QSqlQuery q(m_Database);

    if (!q.prepare(QStringLiteral("SELECT file_name FROM vod_thumbnails WHERE id=?"))) {
        qCritical() << "failed to prepare query" << q.lastError();
        return;
    }

    q.addBindValue(r.thumbnail_id);

    if (!q.exec()) {
        qCritical() << "failed to exec query" << q.lastError();
        return;
    }

    if (!q.next()) {
        qDebug() << "thumbnail rowid" << r.thumbnail_id << "gone";
        return;
    }

    auto thumbnailFileName = q.value(0).toString();

    if (!q.prepare(QStringLiteral("DELETE FROM vod_thumbnails WHERE id=?"))) {
        qCritical() << "failed to prepare query" << q.lastError();
        return;
    }

    q.addBindValue(r.thumbnail_id);

    if (!q.exec()) {
        qCritical() << "failed to exec query" << q.lastError();
        return;
    }

    qDebug() << "removed thumbnail from database" << r.thumbnail_id;

    auto filePath = m_SharedState->m_ThumbnailDir + thumbnailFileName;
    if (QFile::remove(filePath)) {
        qDebug() << "removed thumbnail file" << filePath;
    }

    emit thumbnailUnavailable(r.url_share_id, TE_ContentGone);
}

void ScVodDataManagerWorker::maxConcurrentMetaDataDownloadsChanged(int value)
{
    m_Vodman->setMaxConcurrentMetaDataDownloads(value);
}

void
ScVodDataManagerWorker::fetchTitle(qint64 urlShareId) {
    QSqlQuery q(m_Database);

    static const QString s_Sql = QStringLiteral(
                "SELECT title FROM vod_url_share WHERE id=?");

    if (!q.prepare(s_Sql)) {
        qCritical() << "failed to prepare query" << q.lastError();
        return;
    }

    q.addBindValue(urlShareId);

    if (!q.exec()) {
        qCritical() << "failed to exec query" << q.lastError();
        return;
    }

    if (q.next()) {
        emit titleAvailable(urlShareId, q.value(0).toString());
    }
}

void
ScVodDataManagerWorker::fetchSeen(qint64 rowid, const QString& table, const QString& where)
{
    if (table.isEmpty()) {
        qWarning() << "no table";
        return;
    }

    if (where.isEmpty()) {
        qWarning() << "no filters";
        return;
    }

    QSqlQuery q(m_Database);


    static const QString sql = QStringLiteral(
                "SELECT\n"
                "    SUM(seen)/(1.0 * COUNT(seen))\n"
                "FROM\n"
                "    %1\n"
                "%2\n");

    if (!q.exec(sql.arg(table, where))) {
        qCritical() << "failed to exec query" << q.lastError();
        return;
    }

    q.next();

    emit seenAvailable(rowid, q.value(0).toReal());
}

void
ScVodDataManagerWorker::fetchVodEnd(qint64 rowid, int startOffsetS, int vodLengthS)
{
    static const QString sql = QStringLiteral(
"SELECT\n"
"   MIN(offset)\n"
"FROM\n"
"   offline_vods\n"
"WHERE\n"
"   offset>?\n"
"   AND vod_url_share_id IN (SELECT vod_url_share_id FROM offline_vods WHERE id=?)\n");

    QSqlQuery q(m_Database);
    if (!q.prepare(sql)) {
        qCritical() << "failed to prepare query" << q.lastError();
        return;
    }

    q.addBindValue(startOffsetS);
    q.addBindValue(rowid);

    if (!q.exec()) {
        qCritical() << "failed to exec query" << q.lastError();
        return;
    }

    if (q.next()) {
        auto v = q.value(0);
        if (!v.isNull()) {
            emit vodEndAvailable(rowid, v.toInt());
            return;
        }
    }

    emit vodEndAvailable(rowid, vodLengthS);
}

void
ScVodDataManagerWorker::notifyVodDownloadsChanged()
{
    QSqlQuery q(m_Database);
    ScVodIdList downloads;
    downloads.reserve(m_VodmanFileRequests.size());

    auto beg = m_VodmanFileRequests.cbegin();
    auto end = m_VodmanFileRequests.cend();
    for (auto it = beg; it != end; ++it) {
        const VodmanFileRequest& r = it.value();

        if (Q_UNLIKELY(!selectIdFromVodsWhereUrlShareIdEquals(q, r.vod_url_share_id))) {
            return;
        }

        while (q.next()) {
            auto vodId = qvariant_cast<qint64>(q.value(0));
            downloads << vodId;
        }
    }

    emit vodDownloadsChanged(downloads);
}


void
ScVodDataManagerWorker::databaseStoreCompleted(int token, qint64 insertId, int error, QString errorDescription)
{
    auto it = m_PendingDatabaseStores.find(token);
    if (it == m_PendingDatabaseStores.end()) {
        return;
    }

    auto callback = it.value();
    m_PendingDatabaseStores.erase(it);

    if (error) {
        qWarning() << "database insert/update/delete failed code" << error << "desc" << errorDescription;
        callback(insertId, true);
    } else {
        callback(insertId, false);
    }
}

void
ScVodDataManagerWorker::setYtdlPath(const QString& path)
{
    m_Vodman->setYtdlPath(path);
}

void
ScVodDataManagerWorker::clearYtdlCache()
{
    m_Vodman->clearYtdlCache();
}
