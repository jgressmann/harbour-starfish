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
#include <QMutex>
#include <QSqlDatabase>
#include <QSharedPointer>
#include "ScVodman.h"
#include "conq.h"
#include <functional>

class QNetworkReply;
class QNetworkAccessManager;
class VMVodFileDownload;
class ScVodDataManagerState;

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
    void fetchVod(qint64 rowid, int formatIndex, bool implicitlyStarted);
    void queryVodFiles(qint64 rowid);
    void cancelFetchVod(qint64 rowid);
    void cancelFetchMetaData(qint64 rowid);
    void cancelFetchThumbnail(qint64 rowid);

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
    void vodDownloadsChanged();

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
        bool implicitlyStarted;
    };

    struct ThumbnailRequest {
        qint64 rowid;
    };



private slots:
    void onMetaDataDownloadCompleted(qint64 token, const VMVod& vod);
    void onFileDownloadChanged(qint64 token, const VMVodFileDownload& download);
    void onFileDownloadCompleted(qint64 token, const VMVodFileDownload& download);
    void onDownloadFailed(qint64 token, int serviceErrorCode);
    void requestFinished(QNetworkReply* reply);

private:
    void fetchMetaData(qint64 urlShareId, const QString& url);
    void updateVodDownloadStatus(qint64 urlShareId, const VMVodFileDownload& download);
    void thumbnailRequestFinished(QNetworkReply* reply, ThumbnailRequest& r);
    void fetchThumbnailFromUrl(qint64 rowid, const QString& url);
    void addThumbnail(qint64 rowid, const QByteArray& bytes);


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
