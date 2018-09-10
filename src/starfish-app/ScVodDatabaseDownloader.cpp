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

#include "ScVodDatabaseDownloader.h"
#include "ScVodDataManager.h"
#include "Sc.h"
#include "ScApp.h"

#include <QNetworkReply>
#include <QDebug>
#include <QDomDocument>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QJsonArray>




ScVodDatabaseDownloader::~ScVodDatabaseDownloader() {
    cancel();
    while (true) {
        auto done = false;
        m_Lock.lock();
        done = m_PendingActions == 0;
        m_Lock.unlock();

        if (done) {
            break;
        }

        QThread::msleep(100);
    }
}

ScVodDatabaseDownloader::ScVodDatabaseDownloader(QObject *parent)
    : QObject(parent)
    , m_Lock(QMutex::Recursive)
{
    m_Status = Status_Finished;
    m_Error = Error_None;
    m_Progress = 0;
    m_YearMin = -1;
    m_YearMax = -1;
    m_DownloadState = DS_None;
    m_PendingActions = 0;
    m_ProgressIsIndeterminate = false;
    m_Scraper = nullptr;
    m_DataManager = nullptr;
    m_Reply = nullptr;

    connect(&m_NetworkAccessManager, &QNetworkAccessManager::finished,
            this, &ScVodDatabaseDownloader::onRequestFinished);
}

void
ScVodDatabaseDownloader::setStatus(Status status) {
    if (m_Status != status) {
        m_Status = status;
        emit statusChanged();
    }
}

void
ScVodDatabaseDownloader::setError(Error error) {
    if (m_Error != error) {
        m_Error = error;
        emit errorChanged();
    }
}

void ScVodDatabaseDownloader::_downloadNew()
{
    ++m_PendingActions;

    auto marker = m_DataManager->downloadMarker();

    if (m_YearMin == -1) {
        setProgressDescription(QStringLiteral("Fetching database metadata"));
        auto url = QStringLiteral("https://www.dropbox.com/s/kahfxt2qqpve424/database.json?dl=1");
        m_Reply = m_NetworkAccessManager.get(Sc::makeRequest(url));
        m_DownloadState = DS_FetchingMetaData;
        m_LimitMarker = marker;
    } else if (m_YearMin == -2) {
        // can't fetch or bad format
        m_DownloadState = DS_Scraping;
        m_Scraper->startFetch();
    } else {
        QString url;
        int year = 0;
        for (int y = m_YearMin, i = 0; y <= m_YearMax; ++y, ++i) {
            if (marker.year() <= y) {
                url = m_YearUrls[i];
                if (url.isEmpty()) {
                    continue;
                }
                year = y;
                break;
            }
        }

        if (year) {
            setProgress(0);
            m_TargetMarker = QDate(year+1, 1, 1);
        } else {
            setProgressDescription(QStringLiteral("Fetching new VODs"));
            m_TargetMarker = QDate();
            m_LimitMarker = marker;
        }

        if (url.isEmpty()) {
            m_DownloadState = DS_Scraping;
            m_Scraper->startFetch();
        } else {
            m_DownloadState = DS_FetchingDatabases;
            m_Reply = m_NetworkAccessManager.get(Sc::makeRequest(url));
        }
    }

    updateProgressDescription();
    emit progressIndeterminateChanged();
    emit canDownloadNewChanged();
}

void
ScVodDatabaseDownloader::skip() {
    QMutexLocker g(&m_Lock);
    if (!m_Scraper) {
        qCritical() << "scraper not set";
        return;
    }

    m_Scraper->skip();
}

void
ScVodDatabaseDownloader::downloadNew() {
    QMutexLocker g(&m_Lock);
    switch (m_Status) {
    case Status_Finished:
    case Status_Error:
    case Status_Canceled:
        break;
    case Status_Downloading:
        qWarning() << "download already in progress";
        return;
    default:
        qCritical() << "unhandled status" << m_Status;
        return;
    }

    if (!m_Scraper) {
        qWarning() << "no scraper set";
        setError(Error_InvalidParam);
        setStatus(Status_Error);
        return;
    }

    if (!m_DataManager) {
        qWarning() << "no database set";
        setError(Error_InvalidParam);
        setStatus(Status_Error);
        return;
    }

    setError(Error_None);
    setStatus(Status_Downloading);
    setProgress(0);

    _downloadNew();
}


void
ScVodDatabaseDownloader::onRequestFinished(QNetworkReply* reply) {
    QMutexLocker guard(&m_Lock);
    reply->deleteLater();
    m_Reply = nullptr;

    switch (reply->error()) {
    case QNetworkReply::OperationCanceledError:
        break;
    case QNetworkReply::NoError: {
        // Get the http status code
        int v = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        if (v >= 200 && v < 300) { // Success
            auto bytes = reply->readAll();
            qDebug("%s\n", qPrintable(bytes));

            if (m_YearMin == -1) {
                // try parse json
                if (tryParseDatabaseUrls(bytes)) {
                    // nop
                } else {
                    setError(Error_DataInvalid);
                    setStatus(Status_Error);
                    m_YearMin = -2;
                }
            } else {
                // must be the xml database
                bool ok = false;
                auto xml = Sc::gzipDecompress(bytes, &ok);
                if (ok) {
                    QList<ScRecord> events;
                    if (loadFromXml(xml, &events)) {
                        m_DataManager->setDownloadMarker(m_TargetMarker);
                        ++m_PendingActions;
//                        m_PendingVodAdds += events.size();
                        m_DataManager->queueVodsToAdd(events);
                        actionFinished();
                    } else {
                        setError(Error_DataInvalid);
                        setStatus(Status_Error);
                    }
                } else {
                    setError(Error_DataDecompressionFailed);
                    setStatus(Status_Error);
                }
            }

            if (m_Status == Status_Downloading) {
                _downloadNew();
            }

            updateProgressDescription();
        }
        else if (v >= 300 && v < 400) {// Redirection

            // Get the redirection url
            auto newUrl = reply->attribute(QNetworkRequest::RedirectionTargetAttribute).toUrl();
            // Because the redirection url can be relative,
            // we have to use the previous one to resolve it
            newUrl = reply->url().resolved(newUrl);

            ++m_PendingActions;
            m_Reply = m_NetworkAccessManager.get(Sc::makeRequest(newUrl));
        } else  {
            qDebug() << "Http status code:" << v;
            setError(Error_NetworkFailure);
            setStatus(Status_Error);
        }
    } break;
    default:
        qDebug() << "Network request failed: " << reply->errorString();
        setError(Error_NetworkFailure);
        setStatus(Status_Error);
        break;
    }

    actionFinished();
}

bool
ScVodDatabaseDownloader::loadFromXml(const QByteArray& xml, QList<ScEvent>* events) {
    events->clear();

    QDomDocument doc;
    doc.setContent(xml);
    auto version = doc.documentElement().attribute(QStringLiteral("version")).toInt();
    auto eventsNode = doc.documentElement().namedItem(QStringLiteral("events")).childNodes();
    for (int i = 0; i < eventsNode.size(); ++i) {
        auto eventNode = eventsNode.item(i);
        ScEvent event = ScEvent::fromXml(eventNode, version);
        if (!event.isValid()) {
            qWarning() << "invalid event after xsd validation";
            setError(Error_DataInvalid);
            setStatus(Status_Error);
            return false;
        }

        *events << event;
    }

    return true;
}


bool
ScVodDatabaseDownloader::loadFromXml(const QByteArray& xml, QList<ScRecord>* records) {
    Q_ASSERT(records);
    records->clear();

    QDomDocument doc;
    doc.setContent(xml);
//    auto version = doc.documentElement().attribute(QStringLiteral("version")).toInt();
    auto eventsNode = doc.documentElement().namedItem(QStringLiteral("records")).childNodes();
    for (int i = 0; i < eventsNode.size(); ++i) {
        auto eventNode = eventsNode.item(i);
        auto record = ScRecord::fromXml(eventNode);
        if (!record.valid) {
            qWarning() << "invalid record after xsd validation";
            setError(Error_DataInvalid);
            setStatus(Status_Error);
            return false;
        }

        *records << record;
    }

    return true;
}

void
ScVodDatabaseDownloader::setScraper(ScVodScraper* scraper) {
    QMutexLocker g(&m_Lock);
    if (m_Status == Status_Downloading) {
        qCritical() << "refusing to change scraper during download";
        return;
    }

    if (m_Scraper != scraper) {
        if (m_Scraper) {
            m_Scraper->setClassifier(nullptr);
//            disconnect(m_Scraper, &ScVodScraper::errorChanged, this, &ScQuickDatabaseDownloader::onScraperErrorChanged);
            disconnect(m_Scraper, &ScVodScraper::statusChanged, this, &ScVodDatabaseDownloader::onScraperStatusChanged);
            disconnect(m_Scraper, &ScVodScraper::progressChanged, this, &ScVodDatabaseDownloader::onScraperProgressChanged);
            disconnect(m_Scraper, &ScVodScraper::progressDescriptionChanged, this, &ScVodDatabaseDownloader::onScraperProgressDescriptionChanged);
            disconnect(m_Scraper, &ScVodScraper::excludeEvent, this, &ScVodDatabaseDownloader::excludeEvent);
            disconnect(m_Scraper, &ScVodScraper::excludeStage, this, &ScVodDatabaseDownloader::excludeStage);
            disconnect(m_Scraper, &ScVodScraper::excludeMatch, this, &ScVodDatabaseDownloader::excludeMatch);
            disconnect(m_Scraper, &ScVodScraper::excludeRecord, this, &ScVodDatabaseDownloader::excludeRecord);
            disconnect(m_Scraper, &ScVodScraper::hasRecord, this, &ScVodDatabaseDownloader::hasRecord);
        }

        m_Scraper = scraper;
        emit scraperChanged();

        if (m_Scraper) {
            if (m_DataManager) {
                m_Scraper->setClassifier(m_DataManager->classifier());
            }
//            connect(m_Scraper, &ScVodScraper::errorChanged, this, &ScQuickDatabaseDownloader::onScraperErrorChanged);
            connect(m_Scraper, &ScVodScraper::statusChanged, this, &ScVodDatabaseDownloader::onScraperStatusChanged);
            connect(m_Scraper, &ScVodScraper::progressChanged, this, &ScVodDatabaseDownloader::onScraperProgressChanged);
            connect(m_Scraper, &ScVodScraper::progressDescriptionChanged, this, &ScVodDatabaseDownloader::onScraperProgressDescriptionChanged);
            connect(m_Scraper, &ScVodScraper::excludeEvent, this, &ScVodDatabaseDownloader::excludeEvent);
            connect(m_Scraper, &ScVodScraper::excludeStage, this, &ScVodDatabaseDownloader::excludeStage);
            connect(m_Scraper, &ScVodScraper::excludeMatch, this, &ScVodDatabaseDownloader::excludeMatch);
            connect(m_Scraper, &ScVodScraper::excludeRecord, this, &ScVodDatabaseDownloader::excludeRecord);
            connect(m_Scraper, &ScVodScraper::hasRecord, this, &ScVodDatabaseDownloader::hasRecord);
        }
    }
}

void
ScVodDatabaseDownloader::excludeEvent(const ScEvent& event, bool* exclude) {
    QMutexLocker g(&m_Lock);

    Q_ASSERT(exclude);

    if (event.year() < m_LimitMarker.year()) {
        *exclude = true;
    } else {
        m_DataManager->excludeEvent(event, exclude);
    }
}

void
ScVodDatabaseDownloader::excludeStage(const ScStage& stage, bool* exclude) {
    QMutexLocker g(&m_Lock);
    Q_ASSERT(exclude);

    m_DataManager->excludeStage(stage, exclude);
}

void
ScVodDatabaseDownloader::excludeMatch(const ScMatch& match, bool* exclude) {
    QMutexLocker g(&m_Lock);
    Q_ASSERT(exclude);

    if (match.date() < m_LimitMarker) {
        *exclude = true;
    } else {
        m_DataManager->excludeMatch(match, exclude);
    }
}

void
ScVodDatabaseDownloader::excludeRecord(const ScRecord& record, bool* exclude) {
    QMutexLocker g(&m_Lock);
    Q_ASSERT(exclude);
//    if (record.valid & ScRecord::ValidYear) {
//        if (record.year < m_LimitMarker.year()) {
//            *exclude = true;
//            m_Scraper->cancelFetch();
//            return;
//        }
//    }

    if (record.isValid(ScRecord::ValidMatchDate | ScRecord::ValidYear)) {
        if (record.matchDate < m_LimitMarker &&
                // don't abort on events that cross year boundaries
                record.matchDate.year() == record.year) {
            *exclude = true;
            m_Scraper->cancelFetch();
            return;
        }
    }

    m_DataManager->hasRecord(record, exclude);
}

void
ScVodDatabaseDownloader::hasRecord(const ScRecord& /*record*/, bool* exists) {
    QMutexLocker g(&m_Lock);
    Q_ASSERT(exists);
    *exists = false; // if user cancelled we'll never get older VODs for the current year
}

void
ScVodDatabaseDownloader::onScraperStatusChanged() {
    QMutexLocker g(&m_Lock);
    qDebug("scrape status %d\n", m_Scraper->status());
    switch (m_Scraper->status()) {
    case ScVodScraper::Status_VodFetchingComplete:
    case ScVodScraper::Status_VodFetchingCanceled:
    case ScVodScraper::Status_Error: {
        m_DataManager->queueVodsToAdd(m_Scraper->vods());
        actionFinished();
    }   break;
    default:
        break;
    }

    updateProgressDescription();
}

void
ScVodDatabaseDownloader::onScraperProgressChanged() {
    QMutexLocker g(&m_Lock);
    updateProgressDescription();
}

void
ScVodDatabaseDownloader::onScraperProgressDescriptionChanged() {
    QMutexLocker g(&m_Lock);

    updateProgressDescription();
}

void
ScVodDatabaseDownloader::cancel() {
    QMutexLocker g(&m_Lock);

    qDebug() << "cancel";

    if (m_Reply) {
        m_Reply->abort();
    }

    switch (m_Status) {
    case Status_Downloading:
        m_DownloadState = DS_Canceling;
        if (m_Scraper) {
            m_Scraper->cancelFetch();
        }
        break;
    default:
        break;
    }
}

void
ScVodDatabaseDownloader::setProgress(qreal value) {
    if (m_Progress != value) {
        m_Progress = value;
        emit progressChanged();
    }
}

void
ScVodDatabaseDownloader::setProgressDescription(const QString& value) {
    if (m_ProgressDescription != value) {
        m_ProgressDescription = value;
        emit progressDescriptionChanged();
    }
}

bool
ScVodDatabaseDownloader::progressIndeterminate() const {
    return m_DownloadState != DS_Scraping;
}

bool
ScVodDatabaseDownloader::tryParseDatabaseUrls(const QByteArray& bytes) {
    QJsonParseError error;
    QJsonDocument doc = QJsonDocument::fromJson(bytes, &error);
    if (error.error != QJsonParseError::NoError) {
        qCritical() << "JSON parse error:" << error.errorString();
        return false;
    }

    QJsonObject root = doc.object();
    if (root[QStringLiteral("version")].toInt() != 1) {
        qCritical() << "No version or version mismatch";
        return false;
    }

    m_YearMin = root[QStringLiteral("year_min")].toInt();
    m_YearMax = root[QStringLiteral("year_max")].toInt();
    auto urls = root[QStringLiteral("year_urls")].toArray();
    m_YearUrls.clear();
    for (const auto item : urls) {
        m_YearUrls << item.toString();
    }

    if (m_YearMin <= 0 || m_YearMax <= 0 ||
        m_YearMin > m_YearMax || m_YearMax - m_YearMin + 1 > m_YearUrls.size()) {
        qCritical() << "Ill-formed data. Year min" << m_YearMin << "year max" << m_YearMax << "# urls" << m_YearUrls.size();
        return false;
    }

    return true;
}

void
ScVodDatabaseDownloader::actionFinished() {
    QMutexLocker g(&m_Lock);
    if (--m_PendingActions == 0) {
        if (m_Status == Status_Downloading) {
            setStatus(m_DownloadState == DS_Canceling ? Status_Canceled : Status_Finished);
        }

        emit canDownloadNewChanged();
    }
}

void
ScVodDatabaseDownloader::setDataManager(ScVodDataManager* value) {
    QMutexLocker g(&m_Lock);
    if (m_Status == Status_Downloading) {
        qCritical() << "refusing to change data manager during download";
        return;
    }

    if (m_DataManager != value) {
        if (m_Scraper) {
            m_Scraper->setClassifier(nullptr);
        }

        m_DataManager = value;
        emit dataManagerChanged();

        if (m_DataManager) {
            if (m_Scraper) {
                m_Scraper->setClassifier(m_DataManager->classifier());
            }
        }
    }
}

bool
ScVodDatabaseDownloader::canDownloadNew() const {
    QMutexLocker g(&m_Lock);
    if (m_Status == Status_Downloading) {
        return false;
    }

    if (m_PendingActions > 0) {
        return false;
    }

    return true;
}

void
ScVodDatabaseDownloader::updateProgressDescription() {
    switch (m_Status) {
    case Status_Downloading:
        switch (m_DownloadState) {
        case DS_FetchingMetaData:
            setProgressIndeterminate(true);
            setProgressDescription(QStringLiteral("Fetching meta data"));
            break;
        case DS_FetchingDatabases:
            setProgressIndeterminate(true);
            setProgressDescription(QString::asprintf("Fetching VODs for %d", m_TargetMarker.year()-1));
            break;
        case DS_Scraping:
            setProgressIndeterminate(false);
            setProgress(m_Scraper->progress());
            setProgressDescription(m_Scraper->progressDescription());
            break;
        case DS_Canceling:
            setProgressIndeterminate(true);
            setProgressDescription(QStringLiteral("Adding VODs"));
            break;
        default:
            setProgressIndeterminate(true);
            setProgressDescription("FIX ME");
            break;
        }
        break;
    default:
        setProgressIndeterminate(true);
        setProgress(0);
        setProgressDescription(QString());
        break;
    }
}

void
ScVodDatabaseDownloader::setProgressIndeterminate(bool value) {
    if (m_ProgressIsIndeterminate != value) {
        m_ProgressIsIndeterminate = value;
        emit progressIndeterminateChanged();
    }
}
