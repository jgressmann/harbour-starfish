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

#include "Vods.h"
#include "ScVodScraper.h"

#include <QObject>
#include <QDate>
#include <QMutex>
#include <QRunnable>
#include <QNetworkAccessManager>

class QNetworkAccessManager;
class QNetworkReply;
class ScScraper;
class ScVodDataManager;
class ScVodDatabaseDownloader : public QObject
{
    Q_OBJECT
    Q_PROPERTY(Status status READ status NOTIFY statusChanged)
    Q_PROPERTY(Error error READ error NOTIFY errorChanged)
    Q_PROPERTY(ScVodScraper* scraper READ scraper WRITE setScraper NOTIFY scraperChanged)
    Q_PROPERTY(ScVodDataManager* dataManager READ dataManager WRITE setDataManager NOTIFY dataManagerChanged)
    Q_PROPERTY(qreal progress READ progress NOTIFY progressChanged)
    Q_PROPERTY(QString progressDescription READ progressDescription NOTIFY progressDescriptionChanged)
    Q_PROPERTY(bool progressIndeterminate READ progressIndeterminate NOTIFY progressIndeterminateChanged)
    Q_PROPERTY(bool canDownloadNew READ canDownloadNew NOTIFY canDownloadNewChanged)

public:
    enum Status {
        Status_Downloading,
        Status_Finished,
        Status_Canceled,
        Status_Error,
    };
    Q_ENUMS(Status)

    enum Error {
        Error_None,
        Error_InvalidParam,
        Error_NetworkFailure,
        Error_DataDecompressionFailed,
        Error_DataInvalid,
    };
    Q_ENUMS(Error)

public:
    ~ScVodDatabaseDownloader();
    explicit ScVodDatabaseDownloader(QObject *parent = nullptr);

public:
    Status status() const { return m_Status; }
    Error error() const { return m_Error; }
    ScVodScraper* scraper() const { return m_Scraper; }
    void setScraper(ScVodScraper* value);
    ScVodDataManager* dataManager() const { return m_DataManager; }
    void setDataManager(ScVodDataManager* value);
    qreal progress() const { return m_Progress; }
    QString progressDescription() const { return m_ProgressDescription; }
    bool progressIndeterminate() const;
    bool canDownloadNew() const;

public slots:
    void downloadNew();
    void cancel();
    void skip();

signals:
    void statusChanged();
    void errorChanged();
    void scraperChanged();
    void dataManagerChanged();
    void progressChanged();
    void progressDescriptionChanged();
    void progressIndeterminateChanged();
    void canDownloadNewChanged();

private slots:
    void onRequestFinished(QNetworkReply* reply);
    void excludeEvent(const ScEvent& event, bool* exclude);
    void excludeStage(const ScStage& stage, bool* exclude);
    void excludeMatch(const ScMatch& match, bool* exclude);
    void excludeRecord(const ScRecord& match, bool* exclude);
    void hasRecord(const ScRecord& match, bool* exists);
    void onScraperStatusChanged();
    void onScraperProgressChanged();
    void onScraperProgressDescriptionChanged();

private:
    void setStatus(Status value);
    void setError(Error value);
    bool loadFromXml(const QByteArray& xml, QList<ScEvent>* events);
    bool loadFromXml(const QByteArray& xml, QList<ScRecord>* records);
    void setProgress(qreal value);
    void setProgressDescription(const QString& value);
    void _downloadNew();
    bool tryParseDatabaseUrls(const QByteArray& bytes);
    void actionFinished();
    void updateProgressDescription();
    void setProgressIndeterminate(bool value);

    enum DownloadState {
        DS_None,
        DS_FetchingMetaData,
        DS_FetchingDatabases,
        DS_Scraping,
        DS_Canceling,
    };

private:
    mutable QMutex m_Lock;
    QNetworkAccessManager m_NetworkAccessManager;
    ScVodScraper* m_Scraper;
    ScVodDataManager* m_DataManager;
    Status m_Status;
    Error m_Error;
    QDate m_TargetMarker;
    QDate m_LimitMarker;
    qreal m_Progress;
    QString m_ProgressDescription;
    int m_YearMin, m_YearMax;
    QList<QString> m_YearUrls;
    int m_PendingActions;
    DownloadState m_DownloadState;
    bool m_ProgressIsIndeterminate;
};


