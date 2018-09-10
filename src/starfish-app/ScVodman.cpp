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

#include "ScVodman.h"
#include <vodman/VMVodMetaDataDownload.h>
#include <vodman/VMVodFileDownload.h>
#include <QDBusConnection>
#include <QMutexLocker>
#include <QDataStream>
#include <QThread>
#include <QCoreApplication>

ScVodman::~ScVodman()
{
    abort();
}

ScVodman::ScVodman(QObject *parent)
    : QObject(parent)
    , m_Lock(QMutex::Recursive)
{
    m_MaxFile = 1;
    m_MaxMeta = 4;
    m_CurrentFile = 0;
    m_CurrentMeta = 0;

    QDBusConnection connection = QDBusConnection::sessionBus();
    m_Service = new org::duckdns::jgressmann::vodman::service("org.duckdns.jgressmann.vodman.service", "/instance", connection);
    m_Service->setParent(this);

    connect(
                m_Service,
                &org::duckdns::jgressmann::vodman::service::vodFileDownloadRemoved,
                this,
                &ScVodman::onVodFileDownloadRemoved);

    connect(
                m_Service,
                &org::duckdns::jgressmann::vodman::service::vodFileDownloadChanged,
                this,
                &ScVodman::onVodFileDownloadChanged);



    connect(
                m_Service,
                &org::duckdns::jgressmann::vodman::service::vodMetaDataDownloadCompleted,
                this,
                &ScVodman::onVodFileMetaDataDownloadCompleted);
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

    QMutexLocker g(&m_Lock);
    Request r;
    r.type = RT_MetaData;
    r.url = url;
    r.formatIndex = -1;
    r.token = -1;


    if (m_MaxMeta > 0 && m_CurrentMeta == m_MaxMeta) {
        m_PendingMetaDataRequests << qMakePair(token, r);
    } else {
        ++m_CurrentMeta;
        issueRequest(token, r);
    }
}

void
ScVodman::issueRequest(qint64 token, const Request& request) {
    m_ActiveRequests.insert(token, request);
    auto reply = m_Service->newToken();
    auto watcher = new QDBusPendingCallWatcher(reply, this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this, &ScVodman::onNewTokenReply);
    m_PendingDBusResponses.insert(watcher, token);
    // no need for this, async calls run on the event loop
    // https://www.qtcentre.org/threads/48450-Problem-with-asyncCall-in-DBus
//    if (reply.isFinished()) {
//        onNewTokenReply(watcher);
//    }
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

    QMutexLocker g(&m_Lock);
    Request r;
    r.type = RT_File;
    r.vod = vod;
    r.formatIndex = formatIndex;
    r.token = -1;
    r.filePath = filePath;

    if (m_MaxMeta > 0 && m_CurrentMeta == m_MaxMeta) {
        m_PendingFileRequests << qMakePair(token, r);
    } else {
        ++m_CurrentFile;
        issueRequest(token, r);
    }
}

void
ScVodman::onVodFileMetaDataDownloadCompleted(qint64 token, const QByteArray& result) {

    qDebug() << "enter" << token;
    QMutexLocker g(&m_Lock);
    auto requestId = m_MetaDataTokenMap.value(token, -1);
    if (requestId >= 0) {

        VMVodMetaDataDownload download;
        {
            QDataStream s(result);
            s >> download;
        }

        qDebug() << "mine" << requestId << download;

        if (download.isValid()) {
            if (download.error() == VMVodEnums::VM_ErrorNone) {
                emit metaDataDownloadCompleted(requestId, download.vod());
            } else {
                emit downloadFailed(requestId, download.error());
            }
        } else {
            if (download.error() != VMVodEnums::VM_ErrorNone) {
                emit downloadFailed(requestId, download.error());
            } else {
                emit downloadFailed(requestId, VMVodEnums::VM_ErrorUnknown);
            }
        }

        m_MetaDataTokenMap.remove(token);
        m_ActiveRequests.remove(requestId);
        --m_CurrentMeta;
        scheduleNextMetaDataRequest();
    } else {
        // nop
    }

    qDebug() << "exit" << token;
}


void
ScVodman::onNewTokenReply(QDBusPendingCallWatcher *self) {
    QMutexLocker g(&m_Lock);
    self->deleteLater();
    QDBusPendingReply<qint64> reply = *self;
    auto requestId = m_PendingDBusResponses.value(self, -1);
    if (requestId >= 0) {
        m_PendingDBusResponses.remove(self);
        auto it = m_ActiveRequests.find(requestId);
        if (it != m_ActiveRequests.end()) {
            Request& r = it.value();
            if (reply.isValid()) {
                r.token = reply.value();
                switch (r.type) {
                case RT_MetaData: {
                    auto reply = m_Service->startFetchVodMetaData(r.token, r.url);
                    auto watcher = new QDBusPendingCallWatcher(reply, this);
                    connect(watcher, &QDBusPendingCallWatcher::finished, this, &ScVodman::onMetaDataDownloadReply);
                    m_PendingDBusResponses.insert(watcher, requestId);
                    m_MetaDataTokenMap.insert(r.token, requestId);
                    // see comment in issueRequest
//                    if (reply.isFinished()) {
//                        onMetaDataDownloadReply(watcher);
//                    }
                } break;
                case RT_File: {
                    VMVodFileDownloadRequest serviceRequest;
                    serviceRequest.filePath = r.filePath;
                    serviceRequest.description = r.vod.description();
                    serviceRequest.format = r.vod.data()._formats[r.formatIndex];
                    QByteArray b;
                    {
                        QDataStream s(&b, QIODevice::WriteOnly);
                        s << serviceRequest;
                    }
                    auto reply = m_Service->startFetchVodFile(r.token, b);
                    auto watcher = new QDBusPendingCallWatcher(reply, this);
                    connect(watcher, &QDBusPendingCallWatcher::finished, this, &ScVodman::onFileDownloadReply);
                    m_PendingDBusResponses.insert(watcher, requestId);
                    m_FileTokenMap.insert(r.token, requestId);
                    // see comment in issueRequest
//                    if (reply.isFinished()) {
//                        onFileDownloadReply(watcher);
//                    }
                } break;
                default:
                    m_ActiveRequests.remove(requestId);
                    emit downloadFailed(requestId, VMVodEnums::VM_ErrorUnknown);
                    break;
                }
            } else {
                 qDebug() << "invalid new token reply" << reply.error();
                 m_ActiveRequests.remove(requestId);
                 switch (r.type) {
                 case RT_MetaData:
                     --m_CurrentMeta;
                     scheduleNextMetaDataRequest();
                     break;
                 case RT_File:
                     --m_CurrentFile;
                     scheduleNextFileRequest();
                     break;
                 }
                 emit downloadFailed(requestId, VMVodEnums::VM_ErrorServiceUnavailable);
            }
        } else {
            qDebug() << "no active request for request id" << requestId;
        }
    } else {
        qDebug() << "no request for DBUS reply" << reply;
    }
}


void
ScVodman::onMetaDataDownloadReply(QDBusPendingCallWatcher *self) {
    QMutexLocker g(&m_Lock);
    self->deleteLater();
    QDBusPendingReply<> reply = *self;
    auto requestId = m_PendingDBusResponses.value(self, -1);
    if (requestId >= 0) {
        m_PendingDBusResponses.remove(self);
        const auto it = m_ActiveRequests.constFind(requestId);
        if (it != m_ActiveRequests.cend()) {
            const Request& r = it.value();
            if (reply.isValid()) {
                // nothing to do
            } else {
                qDebug() << "invalid metadata download reply for" << r.url  << "error" << reply.error();
                m_ActiveRequests.remove(requestId);
                --m_CurrentMeta;
                emit downloadFailed(requestId, VMVodEnums::VM_ErrorServiceUnavailable);
                scheduleNextMetaDataRequest();
            }
        } else {
            // reply is async to event onVodFileMetaDataDownloadCompleted
            //qDebug() << "no active request for request id" << requestId;
        }
    } else {
        qDebug() << "no request for DBUS reply";
    }
}

void
ScVodman::onFileDownloadReply(QDBusPendingCallWatcher *self) {
    QMutexLocker g(&m_Lock);
    self->deleteLater();
    QDBusPendingReply<> reply = *self;
    auto requestId = m_PendingDBusResponses.value(self, -1);
    if (requestId >= 0) {
        m_PendingDBusResponses.remove(self);
        const auto it = m_ActiveRequests.constFind(requestId);
        if (it != m_ActiveRequests.cend()) {
            const Request& r = it.value();
            if (reply.isValid()) {
                // nothing to do
            } else {
                qDebug() << "invalid file download reply for" << r.url  << "error" << reply.error();
                m_ActiveRequests.remove(requestId);
                --m_CurrentFile;
                emit downloadFailed(requestId, VMVodEnums::VM_ErrorServiceUnavailable);
                scheduleNextFileRequest();
            }
        } else {
            // reply is async to event onVodFileDownloadRemoved
            //qDebug() << "no active request for request id" << requestId;
        }
    } else {
        qDebug() << "no request for DBUS reply";
    }
}

qint64
ScVodman::newToken() {
    return m_TokenGenerator++;
}

void
ScVodman::onVodFileDownloadRemoved(qint64 handle, const QByteArray& result)
{
    qDebug() << "enter" << handle;

    QMutexLocker g(&m_Lock);
    auto requestId = m_FileTokenMap.value(handle, -1);
    if (requestId >= 0) {

        VMVodFileDownload download;
        {
            QDataStream s(result);
            s >> download;
        }

        if (download.isValid()) { // valid includes 'no error'
            emit fileDownloadCompleted(requestId, download);
        } else {
            emit downloadFailed(requestId, download.error());
        }

        m_FileTokenMap.remove(handle);
        m_ActiveRequests.remove(requestId);
        --m_CurrentFile;
        scheduleNextFileRequest();
    }

    qDebug() << "exit" << handle;
}

void
ScVodman::onVodFileDownloadChanged(qint64 handle, const QByteArray& result)
{
    qDebug() << "enter" << handle;

    QMutexLocker g(&m_Lock);
    auto requestId = m_FileTokenMap.value(handle, -1);
    if (requestId >= 0) {
        VMVodFileDownload download;
        {
            QDataStream s(result);
            s >> download;
        }

        emit fileDownloadChanged(requestId, download);
    }

    qDebug() << "exit" << handle;
}

void
ScVodman::cancel(qint64 token) {
    QMutexLocker g(&m_Lock);

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

    auto beg = m_FileTokenMap.begin();
    auto end = m_FileTokenMap.end();
    for (auto it = beg; it != end; ++it) {
        if (it.value() == token) {
            m_Service->cancelFetchVodFile(it.key(), false);
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

    QMutexLocker g(&m_Lock);

    if (m_MaxMeta != value) {
        m_MaxMeta = value;
        emit maxConcurrentMetaDataDownloadsChanged();
    }
}

void
ScVodman::abort() {
    {
        QMutexLocker g(&m_Lock);

        // empty pending queues
        m_PendingMetaDataRequests.clear();
        m_PendingFileRequests.clear();

        // abort file fetches
        foreach (auto serviceToken, m_FileTokenMap.keys()) {
            m_Service->cancelFetchVodFile(serviceToken, false);
        }
    }

    // wait for in progress requests to complete
    while (true) {
        {
            QMutexLocker g(&m_Lock);
            if (m_ActiveRequests.isEmpty()) {
                break;
            }

            qDebug() << "# active req" << m_ActiveRequests.size();
            qApp->processEvents();
        }

        QThread::msleep(100);
    }

    delete m_Service;
    m_Service = nullptr;

}
