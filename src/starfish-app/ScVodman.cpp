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

#include "ScVodman.h"



#include <QDebug>

ScVodman::~ScVodman()
{
    qDebug() << "dtor entry";

    qDebug() << "disconnect";
    disconnect();

    // abort file fetches
    const auto beg = m_ActiveFileRequests.cbegin();
    const auto end = m_ActiveFileRequests.cend();
    for (auto it = beg; it != end; ++it) {
        m_Ytdl.cancelFetchPlaylist(it.key(), false);
    }
    qDebug() << "canceled running file fetches";


    qDebug() << "dtor exit";
}

ScVodman::ScVodman(QObject *parent)
    : QObject(parent)
    , m_Ytdl(this)
{
    m_TokenGenerator = 0;
    m_MaxFile = 1;
    m_MaxMeta = 4;
    m_CurrentFile = 0;
    m_CurrentMeta = 0;

    m_Ytdl.setYtdlVerbose(true);

    connect(&m_Ytdl, &VMYTDL::playlistDownloadCompleted, this, &ScVodman::onPlaylistDownloadCompleted);
    connect(&m_Ytdl, &VMYTDL::playlistDownloadChanged, this, &ScVodman::onPlaylistDownloadChanged);
    connect(&m_Ytdl, &VMYTDL::metaDataDownloadCompleted, this, &ScVodman::onMetaDataDownloadCompleted);
}

void
ScVodman::startFetchMetaData(qint64 token, const QString& url) {
    if (token < 0) {
        qWarning() << "invalid token" << token << "meta data fetch not started";
        return;
    }

    if (url.isEmpty()) {
        qWarning() << "invalid url" << url << "meta data fetch not started";
        return;
    }

    m_PendingMetaDataRequests << qMakePair(token, url);
    scheduleNextMetaDataRequest();
}

void
ScVodman::startFetchFile(
        qint64 token,
        const VMPlaylistDownloadRequest& request) {
    if (token < 0) {
        qWarning() << "invalid token" << token << "file fetch not started";
        return;
    }

    if (!request.isValid()) {
        qWarning() << "invalid request";
        return;
    }

    m_PendingFileRequests << qMakePair(token, request);
    scheduleNextFileRequest();
}

void
ScVodman::onMetaDataDownloadCompleted(qint64 token, const VMMetaDataDownload& download) {

    if (download.isValid()) {
        if (download.error() == VMVodEnums::VM_ErrorNone) {
            emit metaDataDownloadCompleted(token, download.playlist());
        } else {
            emit downloadFailed(token, download.error());
        }
    } else {
        if (download.error() != VMVodEnums::VM_ErrorNone) {
            emit downloadFailed(token, download.error());
        } else {
            emit downloadFailed(token, VMVodEnums::VM_ErrorUnknown);
        }
    }

    --m_CurrentMeta;
    scheduleNextMetaDataRequest();
}

qint64
ScVodman::newToken()
{
    return m_TokenGenerator++;
}

void
ScVodman::onPlaylistDownloadCompleted(qint64 token, const VMPlaylistDownload& download)
{
    if (m_ActiveFileRequests.remove(token)) {
        if (download.isValid()) { // valid includes 'no error'
            emit fileDownloadCompleted(token, download);
        } else {
            emit downloadFailed(token, download.error());
        }

        --m_CurrentFile;
        scheduleNextFileRequest();
    }
}

void
ScVodman::onPlaylistDownloadChanged(qint64 handle, const VMPlaylistDownload& download)
{
    if (m_ActiveFileRequests.contains(handle)) {
        emit fileDownloadChanged(handle, download);
    }
}

void
ScVodman::cancel(qint64 token)
{
    auto it = m_ActiveFileRequests.find(token);
    if (it != m_ActiveFileRequests.end()) {
        m_ActiveFileRequests.erase(it);
        m_Ytdl.cancelFetchPlaylist(token, false);
        return;
    }

    for (int i = 0; i < m_PendingFileRequests.size(); ++i) {
        const auto& pair = m_PendingFileRequests[i];
        if (pair.first == token) {
            m_PendingFileRequests.removeAt(i);
            return;
        }
    }

    for (int i = 0; i < m_PendingMetaDataRequests.size(); ++i) {
        const auto& pair = m_PendingMetaDataRequests[i];
        if (pair.first == token) {
            m_PendingMetaDataRequests.removeAt(i);
            return;
        }
    }
}

void
ScVodman::scheduleNextFileRequest() {
    if (!m_PendingFileRequests.isEmpty() &&
        (m_MaxFile <= 0 || m_CurrentFile < m_MaxFile)) {
        auto pair = m_PendingFileRequests.front();
        m_PendingFileRequests.pop_front();
        m_ActiveFileRequests.insert(pair.first, pair.second);
        ++m_CurrentFile;
        m_Ytdl.startFetchPlaylist(pair.first, pair.second);
    }
}

void
ScVodman::scheduleNextMetaDataRequest()
{
    if (!m_PendingMetaDataRequests.isEmpty() &&
        (m_MaxMeta <= 0 || m_CurrentMeta < m_MaxMeta)) {
        auto pair = m_PendingMetaDataRequests.front();
        m_PendingMetaDataRequests.pop_front();
        ++m_CurrentMeta;
        m_Ytdl.startFetchMetaData(pair.first, pair.second);
    }
}

void
ScVodman::setMaxConcurrentMetaDataDownloads(int value)
{
    if (value <= 0) {
        qCritical() << "invalid max meta data downloads" << value;
        return;
    }

    if (m_MaxMeta != value) {
        m_MaxMeta = value;
        emit maxConcurrentMetaDataDownloadsChanged();
    }
}

void
ScVodman::setYtdlPath(const QString& path)
{
    m_Ytdl.setYtdlPath(path);
}

void
ScVodman::clearYtdlCache()
{
    m_Ytdl.clearCache();
}
