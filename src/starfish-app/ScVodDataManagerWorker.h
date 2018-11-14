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

#pragma once

#include <vodman/VMVod.h>
#include <QSqlDatabase>
#include <QSharedPointer>
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


public:
    ~ScVodDataManagerWorker();
    ScVodDataManagerWorker(const QSharedPointer<ScVodDataManagerState>& state);

    int enqueue(Task&& req);
    bool cancel(int ticket);
    void fetchMetaData(qint64 rowid, bool download);
    void fetchThumbnail(qint64 rowid, bool download);
    void fetchVod(qint64 rowid, int formatIndex);
    void queryVodFiles(qint64 rowid);
    void cancelFetchVod(qint64 rowid);
    void cancelFetchMetaData(qint64 rowid);
    void cancelFetchThumbnail(qint64 rowid);
    void fetchTitle(qint64 rowid);
    void fetchSeen(qint64 rowid, const QString& table, const QString& where);
    void fetchVodEnd(qint64 rowid, int startOffsetS, int vodLengthS);

signals:
    void fetchingMetaData(qint64 rowid);
    void fetchingThumbnail(qint64 rowid);
    void metaDataAvailable(qint64 rowid, VMVod vod);
    void metaDataDownloadFailed(qint64 rowid, int error);
    void vodAvailable(
            qint64 rowid,
            QString filePath,
            qreal progress,
            quint64 fileSize,
            int width,
            int height,
            QString formatId);
    void thumbnailAvailable(qint64 rowid, QString filePath);
    void thumbnailDownloadFailed(qint64 rowid, int error, QString url);
    void vodDownloadFailed(qint64 rowid, int error);
    void vodDownloadCanceled(qint64 rowid);
    void titleAvailable(qint64 rowid, QString title);
    void seenAvailable(qint64 rowid, qreal seen);
    void vodEndAvailable(qint64 rowid, int endOffsetS);
    void vodDownloadsChanged(ScVodIdList ids);

public slots:
    void process();
    void maxConcurrentMetaDataDownloadsChanged(int value);

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
    void onDownloadFailed(qint64 token, int serviceErrorCode);
    void requestFinished(QNetworkReply* reply);

private:
    void fetchMetaData(QSqlQuery& q, qint64 urlShareId, const QString& url);
    void updateVodDownloadStatus(qint64 urlShareId, const VMVodFileDownload& download);
    void thumbnailRequestFinished(QNetworkReply* reply, ThumbnailRequest& r);
    void fetchThumbnailFromUrl(qint64 urlShareId, qint64 thumbnailId, const QString& url);
    void addThumbnail(ThumbnailRequest& r, const QByteArray& bytes);
    void notifyVodDownloadsChanged(QSqlQuery& q);
    void notifyFetchingMetaData(QSqlQuery& q, qint64 urlShareId);
    void notifyFetchingThumbnail(QSqlQuery& q, qint64 urlShareId);
    void notifyMetaDataAvailable(QSqlQuery& q, qint64 urlShareId, const VMVod& vod);
    void notifyThumbnailRequestFailed(qint64 urlShareId, int error, const QString& url);

private:
    QSharedPointer<ScVodDataManagerState> m_SharedState;
    ScVodman* m_Vodman;
    QNetworkAccessManager* m_Manager;
    conq::mpscfsq<Task, 256> m_Queue;
    QHash<qint64, VodmanMetaDataRequest> m_VodmanMetaDataRequests;
    QHash<qint64, VodmanFileRequest> m_VodmanFileRequests;
    QHash<QNetworkReply*, ThumbnailRequest> m_ThumbnailRequests;
    QSqlDatabase m_Database;
};
