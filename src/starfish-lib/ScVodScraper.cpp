#include "ScVodScraper.h"

#include <QDebug>
#include <QNetworkAccessManager>

ScVodScraper::~ScVodScraper()
{

}

ScVodScraper::ScVodScraper(QObject* parent)
    : QObject(parent)
    , m_Lock(QMutex::Recursive)
{
    m_Status = Status_Ready;
    m_Error = Error_None;
    m_Progress = 0;
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
    QMutexLocker g(lock());
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

void
ScVodScraper::cancelFetch() {
    QMutexLocker g(lock());
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
    QMutexLocker g(lock());
    return m_Status == Status_VodFetchingInProgress;
}

bool
ScVodScraper::canStartFetch() const {
    QMutexLocker g(lock());
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

QMutex*
ScVodScraper::lock() const {
    return &m_Lock;
}

bool
ScVodScraper::canSkip() const {
    return false;
}
