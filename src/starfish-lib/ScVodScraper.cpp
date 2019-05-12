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

#include "ScVodScraper.h"
#include "Sc.h"

#include <QDebug>
#include <QNetworkAccessManager>

ScVodScraper::~ScVodScraper()
{

}

ScVodScraper::ScVodScraper(QObject* parent)
    : QObject(parent)
{
    m_Status = Status_Ready;
    m_Error = Error_None;
    m_Progress = 0;
    m_Classifier = nullptr;
    m_Year = -1;
}


void
ScVodScraper::setStatus(Status status) {
    if (m_Status != status) {
        m_Status = status;
        emit statusChanged();
        emit canCancelFetchChanged();
        emit canStartFetchChanged();
    }
}

void
ScVodScraper::setProgress(qreal value) {
    if (m_Progress != value) {
        m_Progress = value;
        emit progressChanged();
    }
}

void
ScVodScraper::setError(Error error) {
    if (m_Error != error) {
        m_Error = error;
        emit errorChanged();

    }
}

void
ScVodScraper::setProgressDescription(const QString& newValue) {
    if (newValue != m_ProgressDescription) {
        m_ProgressDescription = newValue;
        emit progressDescriptionChanged();
    }
}

void
ScVodScraper::startFetch() {

    switch (m_Status) {
    case Status_Error:
    case Status_Ready:
    case Status_VodFetchingComplete:
    case Status_VodFetchingCanceled:
        break;
    case Status_VodFetchingInProgress:
        qWarning("Fetch already in progress");
        return;
    case Status_VodFetchingBeingCanceled:
        qWarning("Fetch begin canceled");
        return;
    default:
        qCritical() << "unhandled status" << m_Status;
        return;
    }

    setProgress(0);
    setStatus(Status_VodFetchingInProgress);
    _fetch();
}

//void
//ScVodScraper::cancelFetch(Cancelation cancelation) {

//    switch (m_Status) {
//    case Status_VodFetchingInProgress:
//        setStatus(Status_VodFetchingBeingCanceled);
//        _cancel(cancelation);
//        break;
//    default:
//        break;
//    }
//}

//void
//ScVodScraper::_cancel(Cancelation) {
//    setStatus(Status_VodFetchingCanceled);
//}

void
ScVodScraper::cancelFetch() {

    switch (m_Status) {
    case Status_VodFetchingInProgress:
        setStatus(Status_VodFetchingBeingCanceled);
        _cancel();
        break;
    default:
        break;
    }
}


void
ScVodScraper::_cancel() {
    setStatus(Status_VodFetchingCanceled);
}

bool
ScVodScraper::canCancelFetch() const {

    return m_Status == Status_VodFetchingInProgress;
}

bool
ScVodScraper::canStartFetch() const {

    switch (m_Status) {
    case Status_Error:
    case Status_Ready:
    case Status_VodFetchingComplete:
    case Status_VodFetchingCanceled:
        return true;

    default:
        return false;
    }
}

bool
ScVodScraper::_canSkip() const {
    return false;
}

bool
ScVodScraper::canSkip() const {
    return _canSkip();
}

void
ScVodScraper::skip() {

    if (_canSkip()) {
        _skip();
    } else {
        qWarning("scraper %s doesn't support skipping\n", qPrintable(_id()));
    }
}

void
ScVodScraper::_skip() {

}

//void
//ScVodScraper::abort() {
//    qDebug() << id() << "abort enter";
//    cancelFetch(Cancelation_Immediately);
//    qDebug() << id() << "abort exit";
//}

void
ScVodScraper::abort() {
    qDebug() << id() << "abort enter";
    cancelFetch();
    qDebug() << id() << "abort exit";
}

void
ScVodScraper::setStateFilePath(const QString& value)
{
    if (value != m_StateFilePath) {
        m_StateFilePath = value;
        emit stateFilePathChanged();
    }
}

QNetworkReply*
ScVodScraper::makeRequest(const QUrl& url, const QString& referer) const
{
    auto req = Sc::makeRequest(url);
    if (!referer.isEmpty()) {
        req.setRawHeader("Referer", referer.toUtf8());
    }

    return m_Manager.get(req);
}
