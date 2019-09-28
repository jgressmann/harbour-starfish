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
#include "VMDownload.h"
#include <QSqlError>
#include <QSqlQuery>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QFile>
#include <QFileInfo>
#include <QDataStream>
#include <QTemporaryFile>
#include <QMimeDatabase>
#include <QUrl>
#include <QUrlQuery>
#include <QJsonObject>
#include <QJsonDocument>


namespace
{
inline bool selectIdFromVodsWhereUrlShareIdEquals(QSqlQuery& q, qint64 urlShareId) {
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

inline bool selectUrlShareIdFromVodsWhereIdEquals(QSqlQuery& q, qint64 vodId) {
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

ScVodFileFetchProgress::ScVodFileFetchProgress()
{
    urlShareId = -1;
    fileSize = 0;
    progress = 0;
    width = 0;
    height = 0;
    duration = 0;
    fileIndex = 0;
    fileCount = 0;
}

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
    qRegisterMetaType<VMPlaylist>("VMPlaylist");
    qRegisterMetaType<ScVodIdList>("ScVodIdList");
    qRegisterMetaType<ScVodFileFetchProgress>("ScVodFileFetchProgress");

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
        if (file.open(QIODevice::ReadOnly)) {
            VMPlaylist playlist;
            {
                QDataStream s(&file);
                s >> playlist;
            }

            file.close();

            if (playlist.isValid()) {
                auto expirationDate = getExpirationDate(playlist, fi.created());
                if (expirationDate <= QDateTime::currentDateTime()) {
                    file.remove();
                    if (download) {
                        emit fetchingMetaData(urlShareId);
                        fetchMetaData(urlShareId, url);
                    } else {
                        emit metaDataUnavailable(urlShareId);
                    }
                } else {
                    emit metaDataAvailable(urlShareId, playlist, expirationDate);
                }
            } else {
                // read invalid playlist, try again
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
                "SELECT thumbnail_file_name FROM vod_url_share WHERE id=?");
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

    auto fileName = q.value(0).toString();
    if (fileName.isEmpty()) {
        if (download) {
            // continue
        } else {
            // don't try to help, it will mess up meta data fetch logic
            emit thumbnailUnavailable(urlShareId, TE_Other);
            return;
        }
    } else {
        auto thumbNailFilePath = m_SharedState->m_ThumbnailDir + fileName;
        QFileInfo fi(thumbNailFilePath);
        if (fi.exists()) {
            if (fi.size() > 0) {
                emit thumbnailAvailable(urlShareId, thumbNailFilePath);
                return;
            } else {
                // keep empty file so that temp file allocation for
                // other thumbnails doesn't allocate same name multiple times
                if (download) {
                    // continue
                } else {
                    // don't try to help, it will mess up meta data fetch logic
                    emit thumbnailUnavailable(urlShareId, TE_Other);
                }
            }
        }
    }

    auto metaDataFilePath = m_SharedState->m_MetaDataDir + QString::number(urlShareId);
    if (QFileInfo::exists(metaDataFilePath)) {
        QFile file(metaDataFilePath);
        if (file.open(QIODevice::ReadOnly)) {
            VMPlaylist playlist;
            {
                QDataStream s(&file);
                s >> playlist;
            }

            if (playlist.isValid()) {
                auto thumnailUrl = playlist.data().vods[0].thumbnailUrl();
                if (thumnailUrl.isEmpty()) {
                    emit thumbnailUnavailable(urlShareId, TE_Other);

                } else {
                    emit fetchingThumbnail(urlShareId);
                    fetchThumbnailFromUrl(urlShareId, thumnailUrl);
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
}

void
ScVodDataManagerWorker::fetchVod(qint64 urlShareId, const QString& _targetFormat)
{
    auto isUnderway = false;
    for (auto& value : m_VodmanFileRequests) {
        if (value.vod_url_share_id == urlShareId) {
            ++value.refCount;
            isUnderway = true;
        }
    }

    if (isUnderway) {
        return;
    }

    QString targetFormat = _targetFormat;
    QString previousFormat;

    QSqlQuery q(m_Database);

    static const QString sql = QStringLiteral(
                "SELECT\n"
                "    vod_file_name,\n"
                "    progress,\n"
                "    format,\n"
                "    width,\n"
                "    height,\n"
                "    file_index,\n"
                "    (SELECT count(*) FROM vod_files WHERE vod_url_share_id=?) files,\n"
                "   duration\n"
                "FROM vod_files\n"
                "WHERE vod_url_share_id=?\n"
                "ORDER BY file_index ASC\n"
                );

    if (!q.prepare(sql)) {
        qCritical() << "failed to prepare query" << q.lastError() << sql;
        return;
    }

    q.addBindValue(urlShareId);
    q.addBindValue(urlShareId);

    if (!q.exec()) {
        qCritical() << "failed to exec query" << q.lastError();
        return;
    }

    QList<ScVodFileFetchProgress> vodFileProgress;
    int vodFileCount = 0;
    bool fullyDownloaded = true;
    bool createFiles = false;

    if (q.next()) {
        vodFileCount = q.value(6).toInt();
        do {
            const auto fileName = q.value(0).toString();

            ScVodFileFetchProgress fetchProgress;
            fetchProgress.urlShareId = urlShareId;
            fetchProgress.filePath = m_SharedState->m_VodDir + fileName;
            fetchProgress.progress = q.value(1).toFloat();
            fetchProgress.width = q.value(3).toInt();
            fetchProgress.height = q.value(4).toInt();
            fetchProgress.formatId = q.value(2).toString();
            fetchProgress.fileIndex = q.value(5).toInt();
            fetchProgress.fileCount = vodFileCount;
            fetchProgress.duration = q.value(7).toInt();

            QFileInfo fi(fetchProgress.filePath);
            if (fi.exists() && fi.size() > 0) {
                fetchProgress.fileSize = fi.size();
                fullyDownloaded = fullyDownloaded && fetchProgress.progress >= 1;
                previousFormat = fetchProgress.formatId;
            } else {
                fullyDownloaded = false;
            }

            vodFileProgress << fetchProgress;
        } while (q.next());


    } else { // no entries
        createFiles = true;
        fullyDownloaded = false;
    }

    if (fullyDownloaded) {
        for (const auto& progress : vodFileProgress) {
            emit vodAvailable(progress);
        }
        return;
    }


    auto metaDataFilePath = m_SharedState->m_MetaDataDir + QString::number(urlShareId);
    if (QFileInfo::exists(metaDataFilePath)) {
        QFile file(metaDataFilePath);
        if (file.open(QIODevice::ReadOnly)) {
            VMPlaylist playlist;
            {
                QDataStream s(&file);
                s >> playlist;
            }

            if (playlist.isValid()) {
                if (vodFileCount > 0 && vodFileCount != playlist.vods()) {
                    qCritical() << "items in playlist=" << playlist.vods() << "mismatch items in database=" << vodFileCount;
                    emit vodDownloadFailed(urlShareId, VMVodEnums::VM_ErrorUnknown);
                    return;
                }

                auto formats = playlist._vods()[0]._avFormats();
                int formatIndex = -1;
                if (previousFormat.isEmpty()) {
                    for (auto i = 0; i < formats.size(); ++i) {
                        const auto& format = formats[i];
                        if (format.id() == targetFormat) {
                            formatIndex = i;
                            break;
                        }
                    }
                } else {
                    for (auto i = 0; i < formats.size(); ++i) {
                        const auto& format = formats[i];
                        if (format.id() == previousFormat) {
                            qDebug() << "using previous selected format at index" << i << "id" << previousFormat;
                            targetFormat = previousFormat;
                            formatIndex = i;
                            break;
                        }
                    }
                }

                if (-1 == formatIndex) {
                    qWarning() << "format" << targetFormat << "not available, not fetching vod";
                    emit vodDownloadFailed(urlShareId, VMVodEnums::VM_ErrorFormatNotAvailable);
                    return;
                }

                // create files for all vods
                const auto& format = formats[formatIndex];

                if (createFiles) {
                    ScVodFileFetchProgress fetchProgress;
                    fetchProgress.urlShareId = urlShareId;
                    fetchProgress.width = format.width();
                    fetchProgress.height = format.height();
                    fetchProgress.formatId = format.id();
                    fetchProgress.fileCount = playlist.vods();

                    for (auto i = 0; i < playlist.vods(); ++i) {
                        QTemporaryFile tempFile(m_SharedState->m_VodDir + QStringLiteral("XXXXXX.") + format.extension());
                        tempFile.setAutoRemove(false);
                        if (tempFile.open()) {
                            auto vodFilePath = tempFile.fileName();
                            tempFile.close();

                            auto tempFileName = QFileInfo(vodFilePath).fileName();
                            static const QString InsertSql = QStringLiteral(
                                        "INSERT INTO vod_files (vod_file_name, format, width, height, progress, vod_url_share_id, file_index, duration) "
                                        "VALUES (?, ?, ?, ?, ?, ?, ?, ?)");

                            auto tid = m_SharedState->DatabaseStoreQueue->newTransactionId();
                            DatabaseCallback postInsert = [=](qint64 /*vodFileId*/, bool error)
                            {
                                if (error) {
                                    QFile::remove(vodFilePath);
                                }
                            };
                            m_PendingDatabaseStores.insert(tid, postInsert);

                            emit startProcessDatabaseStoreQueue(tid, InsertSql, { tempFileName, format.id(), format.width(), format.height(), 0, urlShareId, i, playlist._vods()[i].duration() });
                            emit startProcessDatabaseStoreQueue(tid, {}, {});

                            // all good, keep file

                            fetchProgress.fileIndex = i;
                            fetchProgress.duration = playlist._vods()[i].duration();
                            fetchProgress.filePath = m_SharedState->m_VodDir + tempFileName;
                            vodFileProgress << fetchProgress;
                        } else {
                            qWarning() << "failed to create temporary file for url share id" << urlShareId;
                            emit vodDownloadFailed(urlShareId, VMVodEnums::VM_ErrorAccess);
                            return;
                        }
                    }
                }

                // create downloads for all files in playlist
                const VMPlaylistData& playlistData = playlist.data();
                for (int i = 0; i < vodFileProgress.size(); ++i) {
                    const auto& fileProgress  =  vodFileProgress[i];
                    if (fileProgress.progress < 1) {
                        const VMVod& vod = playlistData.vods[fileProgress.fileIndex];

                        qDebug() << "start fetch vod file index" << i << "playlist index" << vod.playlistIndex();

                        VodmanFileRequest r;
                        r.token = m_Vodman->newToken();
                        r.vod_url_share_id = urlShareId;
                        r.refCount = 1;
                        r.progress = fileProgress;
                        r.r.filePath = fileProgress.filePath;
                        r.r.format = format.id();
                        r.r.playlist = playlist;
                        r.r.indices << vod.playlistIndex();
                        m_VodmanFileRequests.insert(r.token, r);
                        m_Vodman->startFetchFile(r.token, r.r);
                    }

                    emit vodAvailable(fileProgress);
                }
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

    static const QString sql = QStringLiteral(
                "SELECT\n"
                "    vod_file_name,\n"
                "    progress,\n"
                "    format,\n"
                "    width,\n"
                "    height,\n"
                "    file_index,\n"
                "    (SELECT count(*) FROM vod_files WHERE vod_url_share_id=?) files,\n"
                "    duration\n"
                "FROM vod_files\n"
                "WHERE vod_url_share_id=?\n"
                "ORDER BY file_index ASC\n"
                );

    if (!q.prepare(sql)) {
        qCritical() << "failed to prepare query" << q.lastError() << sql;
        return;
    }

    q.addBindValue(urlShareId);
    q.addBindValue(urlShareId);

    if (!q.exec()) {
        qCritical() << "failed to exec query" << q.lastError();
        return;
    }

    if (q.next()) {
        do {
            const auto fileName = q.value(0).toString();

            ScVodFileFetchProgress fetchProgress;
            fetchProgress.urlShareId = urlShareId;
            fetchProgress.filePath = m_SharedState->m_VodDir + fileName;
            fetchProgress.progress = q.value(1).toFloat();
            fetchProgress.width = q.value(3).toInt();
            fetchProgress.height = q.value(4).toInt();
            fetchProgress.formatId = q.value(2).toString();
            fetchProgress.fileIndex = q.value(5).toInt();
            fetchProgress.fileCount = q.value(6).toInt();
            fetchProgress.duration = q.value(7).toInt();

            QFileInfo fi(fetchProgress.filePath);
            if (fi.exists() && fi.size() > 0) {
                fetchProgress.fileSize = fi.size();
            }

            emit vodAvailable(fetchProgress);
        } while (q.next());
    } else { // no entries
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


void ScVodDataManagerWorker::onMetaDataDownloadCompleted(qint64 token, const VMPlaylist& playlist) {
    auto it = m_VodmanMetaDataRequests.find(token);
    if (it != m_VodmanMetaDataRequests.end()) {
        VodmanMetaDataRequest r = it.value();
        m_VodmanMetaDataRequests.erase(it);

        if (playlist.isValid()) {
            // save vod meta data
            auto metaDataFilePath = m_SharedState->m_MetaDataDir + QString::number(r.vod_url_share_id);
            QFile file(metaDataFilePath);
            if (file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
                QDataStream s(&file);
                s << playlist;
                if (s.status() == QDataStream::Ok && file.flush()) {
                    file.close();

                    qDebug() << "update title, duration for url share id" << r.vod_url_share_id << "to" << playlist.title() << playlist.duration();

                    // send event so that the match page can try to get the thumbnails again
                    auto vodUrlShareId = r.vod_url_share_id;

                    static const QString UpdateUrlShareSql = QStringLiteral(
                                "UPDATE vod_url_share SET title=?, length=? WHERE id=?");
                    static const QString UpdateVodFileSql = QStringLiteral(
                                "UPDATE vod_files SET duration=? WHERE vod_url_share_id=? AND file_index=?");
                    auto tid = m_SharedState->DatabaseStoreQueue->newTransactionId();
                    DatabaseCallback postUpdate = [=](qint64 vodFileId, bool error)
                    {
                        (void)vodFileId;
                        if (error) {
                            emit metaDataDownloadFailed(vodUrlShareId, VMVodEnums::VM_ErrorUnknown);
                        } else {
                            auto expirationTime = getExpirationDate(playlist, QDateTime::currentDateTime());
                            emit metaDataAvailable(vodUrlShareId, playlist, expirationTime);
                        }
                    };
                    m_PendingDatabaseStores.insert(tid, postUpdate);
                    emit startProcessDatabaseStoreQueue(tid, UpdateUrlShareSql, { playlist.title(), playlist.duration(), vodUrlShareId });
                    for (int i = 0; i < playlist.vods(); ++i) {
                        const auto& vod = playlist._vods()[i];
                        emit startProcessDatabaseStoreQueue(tid, UpdateVodFileSql, { vod.duration(), vodUrlShareId, i });
                    }
                    emit startProcessDatabaseStoreQueue(tid, {}, {});
                } else {
                    VMVodEnums::Error e;
                    if (!getVodErrorFromFileError(file, &e)) {
                        e = VMVodEnums::VM_ErrorIo;
                    }
                    emit metaDataDownloadFailed(r.vod_url_share_id, e);
                }
            } else {
                // error
                qDebug() << "failed to open meta data file" << metaDataFilePath << "for writing";
                emit metaDataDownloadFailed(r.vod_url_share_id, VMVodEnums::VM_ErrorAccess);
            }
        } else {
            qDebug() << "token" << token << "invalid vod" << playlist;
            emit metaDataDownloadFailed(r.vod_url_share_id, VMVodEnums::VM_ErrorNoVideo);
        }
    }
}

void ScVodDataManagerWorker::onFileDownloadCompleted(qint64 token, const VMPlaylistDownload& download) {
    auto it = m_VodmanFileRequests.find(token);
    if (it != m_VodmanFileRequests.end()) {
        VodmanFileRequest r = it.value();
        m_VodmanFileRequests.erase(it);

        r.progress.progress = download.data().files[0].progress();
        r.progress.fileSize = download.data().files[0].fileSize();

        emit vodAvailable(r.progress);

        notifyVodDownloadsChanged();
    }
}

void ScVodDataManagerWorker::onFileDownloadChanged(qint64 token, const VMPlaylistDownload& download) {
    decltype(m_VodmanFileRequests)::iterator it = m_VodmanFileRequests.find(token);
    if (it != m_VodmanFileRequests.end()) {
        const VodmanFileRequest& r = it.value();

        static const QString UpdateSql = QStringLiteral(
                    "UPDATE vod_files\n"
                    "SET progress=?\n"
                    "WHERE vod_url_share_id=? and file_index=?\n");

        DatabaseCallback postUpdate = [=](qint64 insertId, bool error)
        {
            (void)insertId;
            if (!error) {
                // download could be cancelled in the meantime
                auto it = m_VodmanFileRequests.find(token);
                if (it != m_VodmanFileRequests.end()) {
                    // guard against reset to zero at restart of canceled download
                    if (download.data().files[0].progress() > 0) {
                        VodmanFileRequest& r = it.value();
                        r.progress.progress = download.data().files[0].progress();
                        r.progress.fileSize = download.data().files[0].fileSize();

                        emit fetchingVod(r.progress);
                    }
                }
            }
        };

        auto tid = m_SharedState->DatabaseStoreQueue->newTransactionId();
        m_PendingDatabaseStores.insert(tid, postUpdate);
        emit startProcessDatabaseStoreQueue(tid, UpdateSql, { download.data().files[0].progress(), r.vod_url_share_id, r.progress.fileIndex });
        emit startProcessDatabaseStoreQueue(tid, {}, {});
    }
}

void
ScVodDataManagerWorker::onDownloadFailed(qint64 token, VMVodEnums::Error serviceErrorCode) {

    qint64 urlShareId = -1;
    auto metaData = false;

    auto it = m_VodmanFileRequests.find(token);
    if (it != m_VodmanFileRequests.end()) {
        VodmanFileRequest r = it.value();
        m_VodmanFileRequests.erase(it);

        cancelFetchVod(r.vod_url_share_id); // cancel any remaining file downloads belonging to same url

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
    for (auto it = m_VodmanFileRequests.begin(); it != m_VodmanFileRequests.end(); ) {
        if (it.value().vod_url_share_id == urlShareId) {
            if (--it.value().refCount == 0) {
                qDebug() << "cancel vod file download token" << it.key() << "for url share id" << urlShareId;
                m_Vodman->cancel(it.key());
                m_VodmanFileRequests.erase(it++);
                canceled = true;
            } else {
                ++it;
            }
        } else {
            ++it;
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
        emit thumbnailUnavailable(r.url_share_id, TE_ContentGone);
    } break;
    default: {
        qDebug() << "Network request failed: " << reply->errorString() << reply->url();
        emit thumbnailDownloadFailed(r.url_share_id, reply->error(), reply->url().toString());
    } break;
    }
}

void ScVodDataManagerWorker::fetchThumbnailFromUrl(qint64 urlShareId, const QString& url) {
    auto beg = m_ThumbnailRequests.begin();
    auto end = m_ThumbnailRequests.end();
    for (auto it = beg; it != end; ++it) {
        if (it.value().url_share_id == urlShareId) {
            ++it.value().refCount;
            return; // already underway
        }
    }

    ThumbnailRequest r;
    r.refCount = 1;
    r.url_share_id = urlShareId;
    m_ThumbnailRequests.insert(m_Manager->get(Sc::makeRequest(url)), r);
}


void
ScVodDataManagerWorker::addThumbnail(ThumbnailRequest& r, const QByteArray& bytes)
{
    auto mimeData = QMimeDatabase().mimeTypeForData(bytes);

    if (!mimeData.isValid()) {
        qDebug() << "no mime data for downloaded thumbnail";
        emit thumbnailDownloadFailed(r.url_share_id, VMVodEnums::VM_ErrorUnsupportedUrl, QString());
        return;
    }

    QSqlQuery q(m_Database);

    if (!q.prepare(QStringLiteral("SELECT thumbnail_file_name FROM vod_url_share WHERE id=?"))) {
        qCritical() << "failed to prepare query" << q.lastError();
        return;
    }

    q.addBindValue(r.url_share_id);

    if (!q.exec()) {
        qCritical() << "failed to exec query" << q.lastError();
        return;
    }

    if (!q.next()) {
        qDebug() << "url_share_id" << r.url_share_id << "gone";
        return;
    }

    auto thumbnailFileName = q.value(0).toString();

    if (thumbnailFileName.isEmpty()) {
        QTemporaryFile tempFile(m_SharedState->m_ThumbnailDir + QStringLiteral("XXXXXX.") + mimeData.preferredSuffix());
        tempFile.setAutoRemove(false);
        if (tempFile.open()) {
            tempFile.close();
            QFileInfo fi(tempFile.fileName());
            thumbnailFileName = fi.fileName();

            auto tid = m_SharedState->DatabaseStoreQueue->newTransactionId();
            emit startProcessDatabaseStoreQueue(
                        tid,
                        QStringLiteral("UPDATE vod_url_share SET thumbnail_file_name=? WHERE id=?"),
                        { thumbnailFileName, r.url_share_id });
            emit startProcessDatabaseStoreQueue(tid, {}, {});
        } else {
            qDebug() << "failed to create thumbnail file";
            VMVodEnums::Error e;
            if (!getVodErrorFromFileError(tempFile, &e)) {
                e = VMVodEnums::VM_ErrorAccess;
            }
            emit thumbnailDownloadFailed(r.url_share_id, e, QString());
            return;
        }
    } else {
        auto lastDot = thumbnailFileName.lastIndexOf(QChar('.'));
        auto suffix = thumbnailFileName.mid(lastDot + 1);
        if (!mimeData.suffixes().contains(suffix)) {
            auto newFileName = thumbnailFileName.left(lastDot) + QStringLiteral(".") + mimeData.preferredSuffix();
            qDebug() << "renaming" << thumbnailFileName << "to" << newFileName;


            auto tid = m_SharedState->DatabaseStoreQueue->newTransactionId();
            emit startProcessDatabaseStoreQueue(
                        tid,
                        QStringLiteral("UPDATE vod_url_share SET thumbnail_file_name=? WHERE id=?"),
                        { newFileName, r.url_share_id });
            emit startProcessDatabaseStoreQueue(tid, {}, {});

            // remove old thumbnail file if it exists
            QFile::remove(m_SharedState->m_ThumbnailDir + thumbnailFileName);

            thumbnailFileName = newFileName;
        }
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
            VMVodEnums::Error e;
            if (!getVodErrorFromFileError(file, &e)) {
                e = VMVodEnums::VM_ErrorIo;
            }
            emit thumbnailDownloadFailed(r.url_share_id, e, QString());
        }
    } else {
        qDebug() << "failed to open" << thumbnailFilePath << "for writing";
        VMVodEnums::Error e;
        if (!getVodErrorFromFileError(file, &e)) {
            e = VMVodEnums::VM_ErrorAccess;
        }
        emit thumbnailDownloadFailed(r.url_share_id, e, QString());
    }
}

void ScVodDataManagerWorker::maxConcurrentMetaDataDownloadsChanged(int value)
{
    m_Vodman->setMaxConcurrentMetaDataDownloads(value);
}

void ScVodDataManagerWorker::maxConcurrentVodFileDownloadsChanged(int value)
{
    m_Vodman->setMaxConcurrentVodFileDownloads(value);
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
"   url_share_vods\n"
"WHERE\n"
"   offset>?\n"
"   AND vod_url_share_id IN (SELECT vod_url_share_id FROM url_share_vods WHERE id=?)\n");

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

QDateTime
ScVodDataManagerWorker::getExpirationDate(const VMPlaylist& playlist, const QDateTime& metaDataFileCreated) const
{
//    QDateTime expirationTime;
    foreach (const auto& vod, playlist._vods()) {
        foreach (const auto& format, vod._avFormats()) {
            QDateTime expirationDate;
            if (tryParseExpirationDateFromUrl(format, &expirationDate)) {
                qDebug() << "expires" << expirationDate.toMSecsSinceEpoch() << expirationDate;
                return expirationDate;
//                if (expirationTime.isValid()) {
//                    expirationTime = qMin(expirationDate, expirationTime);
//                } else {
//                    expirationTime = expirationDate;
//                }
            }
        }

        foreach (const auto& format, vod._videoFormats()) {
            QDateTime expirationDate;
            if (tryParseExpirationDateFromUrl(format, &expirationDate)) {
                qDebug() << "expires" << expirationDate.toMSecsSinceEpoch() << expirationDate;
                return expirationDate;
//                if (expirationTime.isValid()) {
//                    expirationTime = qMin(expirationDate, expirationTime);
//                } else {
//                    expirationTime = expirationDate;
//                }
            }
        }
    }

    return metaDataFileCreated.addSecs(3600); // best guess

//    if (!expirationTime.isValid()) {
//        expirationTime = metaDataFileCreated.addSecs(3600); // best guess
//    }

//    return expirationTime;
}

bool
ScVodDataManagerWorker::tryParseExpirationDateFromUrl(const VMVideoFormat& format, QDateTime* expirationDate) const
{
    Q_ASSERT(expirationDate);

    if (format.streamUrl().indexOf(QStringLiteral(".googlevideo.com/")) > 0) {
        // Google
        // https://r7---sn-8xgn5uxa-i5hl.googlevideo.com/videoplayback?expire=1569676378&ei=-gePXdvEM9ah-gbR0bu4Dw&ip=87.123.201.8&id=o-AKagqf4QO0dT38GZomuqJ5XzKLDiRseTWuz26jkf1nCd&itag=22&source=youtube&requiressl=yes&mm=31%2C29&mn=sn-8xgn5uxa-i5hl%2Csn-i5heen7s&ms=au%2Crdu&mv=m&mvi=6&pl=24&initcwndbps=1741250&mime=video%2Fmp4&ratebypass=yes&dur=3466.530&lmt=1569552472636649&mt=1569654699&fvip=4&fexp=23842630&c=WEB&txp=5535432&sparams=expire%2Cei%2Cip%2Cid%2Citag%2Csource%2Crequiressl%2Cmime%2Cratebypass%2Cdur%2Clmt&sig=ALgxI2wwRQIhAK0YMaNUMB3PJV8iFdzVSdx0pDdqARH3SjPFTidRZIwzAiBLI5N-zPuIkIECVjbGUtkspFBrl44zNItTNQQJV01WZA%3D%3D&lsparams=mm%2Cmn%2Cms%2Cmv%2Cmvi%2Cpl%2Cinitcwndbps&lsig=AHylml4wRQIgf6jZvh3oBSrh7J9UHbWO2fDmdAsJ7TgkKCvT_dyp154CIQDCMMlTNYR0hfdK0Kmj6ZOSJmZW9w2hGof5OwJfMibyJQ%3D%3D

        QUrl u(format.streamUrl());
        QUrlQuery q(u);
        bool ok = false;
        auto expires = q.queryItemValue(QStringLiteral("expire")).toLongLong(&ok);
        if (ok) {
            *expirationDate = QDateTime::fromMSecsSinceEpoch(expires * 1000);
            return true;
        }
    } else if (format.manifestUrl().indexOf(QStringLiteral(".ttvnw.net/")) > 0) {
        // Twitch
        // https://usher.ttvnw.net/vod/464669022.m3u8?allow_audio_only=true&allow_source=true&allow_spectre=true&nauthsig=df59d9b3c5401a904c2d3d477836c89111d56d3b&player=twitchweb&nauth=%7B%22authorization%22%3A%7B%22forbidden%22%3Afalse%2C%22reason%22%3A%22%22%7D%2C%22chansub%22%3A%7B%22restricted_bitrates%22%3A%5B%5D%7D%2C%22device_id%22%3Anull%2C%22expires%22%3A1569729068%2C%22https_required%22%3Afalse%2C%22privileged%22%3Afalse%2C%22user_id%22%3Anull%2C%22version%22%3A2%2C%22vod_id%22%3A464669022%7D
        QUrl u(format.manifestUrl());
        QUrlQuery q(u);
        auto authJson = q.queryItemValue(QStringLiteral("nauth"), QUrl::FullyDecoded);
        QJsonDocument doc = QJsonDocument::fromJson(authJson.toUtf8());
        if (doc.isObject()) {
            auto auth = doc.object();
            bool ok = false;
            auto expires = auth[QStringLiteral("expires")].toVariant().toLongLong(&ok);
            if (ok) {
                *expirationDate = QDateTime::fromMSecsSinceEpoch(expires * 1000);
                return true;
            }
        }
    }

    return false;
}

bool
ScVodDataManagerWorker::getVodErrorFromFileError(const QFile& file, VMVodEnums::Error* error) const
{
    Q_ASSERT(error);
    qDebug() << "file path" << file.fileName() << "error" << file.error() << file.errorString();
    switch (file.error()) {
    case QFile::ResourceError:
        *error = VMVodEnums::VM_ErrorNoSpaceLeftOnDevice;
        return true;
    default:
        return false;
    }
}
