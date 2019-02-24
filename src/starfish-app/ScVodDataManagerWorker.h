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

#pragma once


#include <QSqlDatabase>
#include <QSharedPointer>
#include "VMVod.h"
#include "ScVodman.h"
#include "conq.h"
#include <functional>

class QSqlQuery;
class QNetworkReply;
class QNetworkAccessManager;
class VMVodFileDownload;
class ScVodDataManagerState;

using ScVodIdList = QList<qint64>;

class ScVodDataManagerWorker : public QObject
{
    Q_OBJECT
public:
    using Task = std::function<void()>;
    using DatabaseCallback = std::function<void(qint64 insertId, bool error)>;

    enum ThumbnailError
    {
        TE_MetaDataUnavailable,
        TE_ContentGone,
        TE_Other,
    };
    Q_ENUM(ThumbnailError)


public:
    ~ScVodDataManagerWorker();
    ScVodDataManagerWorker(const QSharedPointer<ScVodDataManagerState>& state);

    int enqueue(Task&& req);
    bool cancel(int ticket);
    void fetchMetaData(qint64 urlShareId, const QString& url, bool download);
    void fetchThumbnail(qint64 urlShareId, bool download);
    void fetchVod(qint64 urlShareId, int formatIndex);
    void queryVodFiles(qint64 urlShareId);
    void cancelFetchVod(qint64 urlShareId);
    void cancelFetchMetaData(qint64 urlShareId);
    void cancelFetchThumbnail(qint64 urlShareId);
    void fetchTitle(qint64 urlShareId);
    void fetchSeen(qint64 rowid, const QString& table, const QString& where);
    void fetchVodEnd(qint64 rowid, int startOffsetS, int vodLengthS);
    void clearYtdlCache();


signals:
    void fetchingMetaData(qint64 urlShareId);
    void fetchingThumbnail(qint64 urlShareId);
    void metaDataAvailable(qint64 urlShareId, VMVod vod);
    void metaDataUnavailable(qint64 urlShareId);
    void metaDataDownloadFailed(qint64 urlShareId, VMVodEnums::Error error);
    void vodAvailable(
            qint64 urlShareId,
            QString filePath,
            qreal progress,
            quint64 fileSize,
            int width,
            int height,
            QString formatId);
    void vodUnavailable(qint64 urlShareId);
    void fetchingVod(
            qint64 urlShareId,
            QString filePath,
            qreal progress,
            quint64 fileSize,
            int width,
            int height,
            QString formatId);
    void thumbnailAvailable(qint64 urlShareId, QString filePath);
    void thumbnailUnavailable(qint64 urlShareId, ScVodDataManagerWorker::ThumbnailError error);
    void thumbnailDownloadFailed(qint64 urlShareId, int error, QString url);
    void vodDownloadFailed(qint64 urlShareId, VMVodEnums::Error error);
    void vodDownloadCanceled(qint64 urlShareId);
    void titleAvailable(qint64 urlShareId, QString title);
    void seenAvailable(qint64 rowid, qreal seen);
    void vodEndAvailable(qint64 rowid, int endOffsetS);
    void vodDownloadsChanged(ScVodIdList ids);
    void startProcessDatabaseStoreQueue(int transactionId, QString sql, QVariantList args);

public slots:
    void process();
    void maxConcurrentMetaDataDownloadsChanged(int value);
    void setYtdlPath(const QString& path);

private:
    struct VodmanMetaDataRequest {
        qint64 vod_url_share_id;
        int refCount;
    };

    struct VodmanFileRequest {
        qint64 token;
        qint64 vod_url_share_id;
        int formatIndex;
        int refCount;
    };

    struct ThumbnailRequest {
        qint64 url_share_id;
        qint64 thumbnail_id;
        int refCount;
    };

private slots:
    void onMetaDataDownloadCompleted(qint64 token, const VMVod& vod);
    void onFileDownloadChanged(qint64 token, const VMVodFileDownload& download);
    void onFileDownloadCompleted(qint64 token, const VMVodFileDownload& download);
    void onDownloadFailed(qint64 token, VMVodEnums::Error serviceErrorCode);
    void requestFinished(QNetworkReply* reply);
    void databaseStoreCompleted(int ticket, qint64 insertId, int error, QString errorDescription);

private:
    void fetchMetaData(qint64 urlShareId, const QString& url);
    void updateVodDownloadStatus(qint64 urlShareId, const VMVodFileDownload& download);
    void thumbnailRequestFinished(QNetworkReply* reply, ThumbnailRequest& r);
    void fetchThumbnailFromUrl(qint64 urlShareId, qint64 thumbnailId, const QString& url);
    void addThumbnail(ThumbnailRequest& r, const QByteArray& bytes);
    void removeThumbnail(ThumbnailRequest& r);
    void notifyVodDownloadsChanged();

private:
    QSharedPointer<ScVodDataManagerState> m_SharedState;
    ScVodman* m_Vodman;
    QNetworkAccessManager* m_Manager;
    conq::mpscfsq<Task, 256> m_Queue;
    QHash<qint64, VodmanMetaDataRequest> m_VodmanMetaDataRequests;
    QHash<qint64, VodmanFileRequest> m_VodmanFileRequests;
    QHash<QNetworkReply*, ThumbnailRequest> m_ThumbnailRequests;
    QHash<int, DatabaseCallback> m_PendingDatabaseStores;
    QSqlDatabase m_Database;
};
