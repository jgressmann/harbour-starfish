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
#include "VMVodMetaDataDownload.h"
#include "VMVodFileDownload.h"

#include <QDebug>

ScVodman::~ScVodman()
{
    qDebug() << "dtor entry";

    qDebug() << "disconnect";
    disconnect();

    // abort file fetches
    const auto beg = m_ActiveRequests.cbegin();
    const auto end = m_ActiveRequests.cend();
    for (auto it = beg; it != end; ++it) {
        if (RT_File == it.value().type) {
            m_Ytdl.cancelFetchVodFile(it.key(), false);
        }
    }
    qDebug() << "canceled running vod file fetches";


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

    connect(&m_Ytdl, &VMYTDL::vodFileDownloadCompleted, this, &ScVodman::onVodFileDownloadRemoved);
    connect(&m_Ytdl, &VMYTDL::vodFileDownloadChanged, this, &ScVodman::onVodFileDownloadChanged);
    connect(&m_Ytdl, &VMYTDL::vodMetaDataDownloadCompleted, this, &ScVodman::onVodFileMetaDataDownloadCompleted);
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

    Request r;
    r.type = RT_MetaData;
    r.url = url;
    r.formatIndex = -1;


    if (m_MaxMeta > 0 && m_CurrentMeta == m_MaxMeta) {
        m_PendingMetaDataRequests << qMakePair(token, r);
    } else {
        ++m_CurrentMeta;
        issueRequest(token, r);
    }
}

void
ScVodman::issueRequest(qint64 token, const Request& request) {
    auto it = m_ActiveRequests.insert(token, request);
    const Request& r = it.value();
    switch (r.type) {
    case RT_MetaData: {
        m_Ytdl.startFetchVodMetaData(token, r.url);
    } break;
    case RT_File: {
        VMVodFileDownloadRequest serviceRequest;
        serviceRequest.filePath = r.filePath;
        serviceRequest.description = r.vod.description();
        serviceRequest.format = r.vod.data()._formats[r.formatIndex];
        m_Ytdl.startFetchVodFile(token, serviceRequest);
    } break;
    default:
        m_ActiveRequests.remove(token);
        emit downloadFailed(token, VMVodEnums::VM_ErrorUnknown);
        break;
    }
}

void
ScVodman::startFetchFile(qint64 token, const VMVod& vod, int formatIndex, const QString& filePath) {
    if (token < 0) {
        qWarning() << "invalid token" << token << "file fetch not started";
        return;
    }

    if (!vod.isValid()) {
        qWarning() << "invalid vod";
        return;
    }

    if (formatIndex < 0 || formatIndex >= vod.formats()) {
        qDebug() << "invalid vod format index"<< "file fetch not started";
        return;
    }

    if (filePath.isEmpty()) {
        qWarning() << "invalid file path" << filePath << "file fetch not started";
        return;
    }

    Request r;
    r.type = RT_File;
    r.vod = vod;
    r.formatIndex = formatIndex;
    r.filePath = filePath;

    if (m_MaxMeta > 0 && m_CurrentMeta == m_MaxMeta) {
        m_PendingFileRequests << qMakePair(token, r);
    } else {
        ++m_CurrentFile;
        issueRequest(token, r);
    }
}

void
ScVodman::onVodFileMetaDataDownloadCompleted(qint64 token, const VMVodMetaDataDownload& download) {

    qDebug() << "enter" << token;
    if (m_ActiveRequests.remove(token)) {
        qDebug() << "mine" << token << download;

        if (download.isValid()) {
            if (download.error() == VMVodEnums::VM_ErrorNone) {
                emit metaDataDownloadCompleted(token, download.vod());
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
    } else {
        // nop
    }

    qDebug() << "exit" << token;
}

qint64
ScVodman::newToken() {
    return m_TokenGenerator++;
}

void
ScVodman::onVodFileDownloadRemoved(qint64 token, const VMVodFileDownload& download)
{
    qDebug() << "enter" << token;

    if (m_ActiveRequests.remove(token)) {
        if (download.isValid()) { // valid includes 'no error'
            emit fileDownloadCompleted(token, download);
        } else {
            emit downloadFailed(token, download.error());
        }

        --m_CurrentFile;
        scheduleNextFileRequest();
    }

    qDebug() << "exit" << token;
}

void
ScVodman::onVodFileDownloadChanged(qint64 handle, const VMVodFileDownload& download)
{
    qDebug() << "enter" << handle;

    if (m_ActiveRequests.contains(handle)) {
        emit fileDownloadChanged(handle, download);
    }

    qDebug() << "exit" << handle;
}

void
ScVodman::cancel(qint64 token)
{
    auto it = m_ActiveRequests.find(token);
    if (it != m_ActiveRequests.end()) {
        auto cancel = RT_File == it.value().type;
        m_ActiveRequests.erase(it);
        if (cancel) {
            m_Ytdl.cancelFetchVodFile(token, false);
        }
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
        ++m_CurrentFile;
        issueRequest(pair.first, pair.second);
    }
}

void
ScVodman::scheduleNextMetaDataRequest() {
    if (!m_PendingMetaDataRequests.isEmpty() &&
        (m_MaxMeta <= 0 || m_CurrentMeta < m_MaxMeta)) {
        auto pair = m_PendingMetaDataRequests.front();
        m_PendingMetaDataRequests.pop_front();
        ++m_CurrentMeta;
        issueRequest(pair.first, pair.second);
    }
}

void
ScVodman::setMaxConcurrentMetaDataDownloads(int value) {
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
