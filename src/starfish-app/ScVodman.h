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

#include "VMYTDL.h"
#include "VMDownload.h"

class VMPlaylistDownload;
class VMMetaDataDownload;

class ScVodman : public QObject
{
    Q_OBJECT
public:
    ~ScVodman();
    explicit ScVodman(QObject* parent = nullptr);

    qint64 newToken();
    void startFetchMetaData(qint64 token, const QString& url);
    void startFetchFile(qint64 token, const VMPlaylistDownloadRequest& request);
    void cancel(qint64 token);
    int maxConcurrentMetaDataDownloads() const { return m_MaxMeta; }
    void setMaxConcurrentMetaDataDownloads(int value);
    int maxConcurrentVodFileDownloads() const { return m_MaxFile; }
    void setMaxConcurrentVodFileDownloads(int value);
    void setYtdlPath(const QString& path);
    void clearYtdlCache();


signals:
    void metaDataDownloadCompleted(qint64 token, const VMPlaylist& playlist);
    void fileDownloadChanged(qint64 token, const VMPlaylistDownload& download);
    void fileDownloadCompleted(qint64 token, const VMPlaylistDownload& download);
    void downloadFailed(qint64 token, VMVodEnums::Error serviceErrorCode);
    void maxConcurrentMetaDataDownloadsChanged();
    void maxConcurrentVodFileDownloadsChanged();

private slots:
    void onPlaylistDownloadCompleted(qint64 handle, const VMPlaylistDownload& download);
    void onPlaylistDownloadChanged(qint64 handle, const VMPlaylistDownload& download);
    void onMetaDataDownloadCompleted(qint64 handle, const VMMetaDataDownload& download);

private:
    void scheduleNextFileRequest();
    void scheduleNextMetaDataRequest();

private:
    VMYTDL m_Ytdl;
    QList<QPair<qint64, VMPlaylistDownloadRequest>> m_PendingFileRequests;
    QList<QPair<qint64, QString>> m_PendingMetaDataRequests;
    QHash<qint64, VMPlaylistDownloadRequest> m_ActiveFileRequests;
    qint64 m_TokenGenerator;
    int m_MaxFile, m_CurrentFile, m_MaxMeta, m_CurrentMeta;
};
