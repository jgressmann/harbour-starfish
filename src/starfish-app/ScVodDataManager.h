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
#include "ScRecord.h"
#include "ScIcons.h"
#include "ScClassifier.h"
#include "ScApp.h"

#include <QMutex>
#include <QSqlDatabase>
#include <QThread>
#include <QVariant>

class QNetworkAccessManager;
class QNetworkRequest;
class QNetworkReply;
class QUrl;
class ScVodman;
class ScEvent;
class ScStage;
class ScMatch;
class VMVodFileDownload;




typedef void (*ScThreadFunction)(void* arg);
class ScWorker : public QObject {
    Q_OBJECT
public:
    ScWorker(ScThreadFunction f, void* arg = nullptr);

signals:
    void finished();

public slots:
    void process();

private:
    ScThreadFunction m_Function;
    void* m_Arg;
};

class ScVodDataManager : public QObject {

    Q_OBJECT
    Q_PROPERTY(Error error READ error NOTIFY errorChanged)
    Q_PROPERTY(ScVodman* vodman READ vodman CONSTANT)
    Q_PROPERTY(QString downloadMarker READ downloadMarkerString NOTIFY downloadMarkerChanged)
    Q_PROPERTY(bool busy READ busy NOTIFY busyChanged)
    Q_PROPERTY(bool ready READ ready NOTIFY readyChanged)
    Q_PROPERTY(QVariant database READ databaseVariant CONSTANT)
    Q_PROPERTY(int vodDownloads READ vodDownloads NOTIFY vodDownloadsChanged)

public:
    enum Error {
        Error_Unknown = -1,
        Error_None,
        Error_CouldntCreateSqlTables,
        Error_CouldntStartTransaction,
        Error_CouldntEndTransaction,
        Error_SqlTableManipError,
    };
    Q_ENUMS(Error)

public:
    ~ScVodDataManager();
    explicit ScVodDataManager(QObject *parent = Q_NULLPTR);

public: //
    Q_INVOKABLE QString label(const QString& key, const QVariant& value = QVariant()) const;
    Q_INVOKABLE QString icon(const QString& key, const QVariant& value = QVariant()) const;
    bool ready() const { return m_Ready; }
    Error error() const { return m_Error; }
    QSqlDatabase database() const { return m_Database; }
    Q_INVOKABLE void fetchMetaData(qint64 rowid);
    Q_INVOKABLE void fetchMetaDataFromCache(qint64 rowid);
    Q_INVOKABLE void fetchThumbnail(qint64 rowid);
    Q_INVOKABLE void fetchThumbnailFromCache(qint64 rowid);
    Q_INVOKABLE QString title(qint64 rowid);
    Q_INVOKABLE void fetchVod(qint64 rowid, int formatIndex, bool implicitlyStarted);
    Q_INVOKABLE void cancelFetchVod(qint64 rowid);
    Q_INVOKABLE void cancelFetchMetaData(qint64 rowid);
    Q_INVOKABLE void queryVodFiles(qint64 rowid);
    Q_INVOKABLE void deleteVod(qint64 rowid);
    Q_INVOKABLE void deleteMetaData(qint64 rowid);
    Q_INVOKABLE void fetchIcons();
    Q_INVOKABLE void fetchClassifier();
    Q_INVOKABLE void resetDownloadMarker();
    void queueVodsToAdd(const QList<ScRecord>& records);
    QDate downloadMarker() const;
    QString downloadMarkerString() const;
    void setDownloadMarker(QDate value);
    void suspendVodsChangedEvents();
    void resumeVodsChangedEvents();
    Q_INVOKABLE qreal seen(const QString& table, const QString& where) const;
    Q_INVOKABLE void setSeen(const QString& table, const QString& where, bool value);
    inline ScVodman* vodman() const { return m_Vodman; }
    inline ScClassifier* classifier() const { return const_cast<ScClassifier*>(&m_Classifier); }
    inline QVariant databaseVariant() const { return QVariant::fromValue(m_Database); }
    bool busy() const;
    Q_INVOKABLE QString makeThumbnailFile(const QString& srcPath);
    Q_INVOKABLE int deleteSeenVodFiles();
    Q_INVOKABLE bool implicitlyStartedVodFetch(qint64 rowid) const;
    int vodDownloads() const;
    Q_INVOKABLE QVariantList vodsBeingDownloaded() const;
    Q_INVOKABLE QString getPersistedValue(const QString& key, const QString& defaultValue = QString()) const;
    Q_INVOKABLE void setPersistedValue(const QString& key, const QString& value);


signals:
    void readyChanged();
    void errorChanged();
    void vodsChanged();
    void fetchingMetaData(qint64 rowid);
    void metaDataAvailable(qint64 rowid, VMVod vod);
    void metaDataDownloadFailed(qint64 rowid, int error);
    void vodAvailable(qint64 rowid, QString filePath, qreal progress, quint64 fileSize, int width, int height, QString formatId);
    void thumbnailAvailable(qint64 rowid, QString filePath);
    void thumbnailDownloadFailed(qint64 rowid, int error, QString url);
    void downloadFailed(QString url, int error, QString filePath);
    void downloadMarkerChanged();
    void busyChanged();
    void vodsToAdd();
    void vodsCleared();
    void vodmanError(int error);
    void vodDownloadFailed(qint64 rowid, int error);
    void vodsAdded(int count);
    void vodDownloadsChanged();



public slots:
    void excludeEvent(const ScEvent& event, bool* exclude);
    void excludeStage(const ScStage& stage, bool* exclude);
    void excludeMatch(const ScMatch& match, bool* exclude);
    void hasRecord(const ScRecord& record, bool* exclude);
    void clear();
    void vacuum();

private:
    enum UrlType {
        UT_Unknown,
        UT_Youtube,
        UT_Twitch,
    };
    enum FetchType {
        FT_MetaData = 0x1,
        FT_File = 0x2,
    };

    struct VodmanMetaDataRequest {
        qint64 token;
        qint64 vod_url_share_id;
    };

    struct VodmanFileRequest {
        qint64 token;
        qint64 vod_url_share_id;
        qint64 vod_file_id;
        int formatIndex;
        bool implicitlyStarted;
    };

    struct ThumbnailRequest {
        qint64 rowid;
    };

    struct IconRequest {
        QString url;
    };

private slots:
    void onMetaDataDownloadCompleted(qint64 token, const VMVod& vod);
    void onFileDownloadChanged(qint64 token, const VMVodFileDownload& download);
    void onFileDownloadCompleted(qint64 token, const VMVodFileDownload& download);
    void onDownloadFailed(qint64 token, int serviceErrorCode);
    void requestFinished(QNetworkReply* reply);
    void vodAddWorkerFinished();

private:
    static bool tryGetEvent(QString& inoutSrc, QString* location);
    static bool tryGetIcon(const QString& title, QString* iconPath);
    void setupDatabase();
    void setReady(bool ready);
    void setError(Error value);
    void updateStatus();
    void setStatusFromRowCount();
    void clearVods();
    void fetchThumbnailFromUrl(qint64 rowid, const QString& url);
    void fetchMetaData(qint64 urlShareId, const QString& url);
    void addThumbnail(qint64 rowid, const QByteArray& bytes);
    static void parseUrl(const QString& url,
                         QString* cannonicalUrl,
                         QString* outId, int* outUrlType, int* outStartOffset);
    static int parseTwitchOffset(const QString& str);
    bool _addVods(const QList<ScRecord>& records);
    void tryRaiseVodsChanged();
    void getSeries(const QString& str, QString& series, QString& event);
    bool exists(
            const ScEvent& event,
            const ScStage& stage,
            const ScMatch& match,
            bool* exists) const;

    bool exists(QSqlQuery& query, const ScRecord& record, qint64* id) const;
    void updateVodDownloadStatus(qint64 vodFileId, const VMVodFileDownload& download);
    void fetchMetaData(qint64 rowid, bool download);
    void iconRequestFinished(QNetworkReply* reply, IconRequest& r);
    void thumbnailRequestFinished(QNetworkReply* reply, ThumbnailRequest& r);
    static void batchAddVods(void* ctx);
    void batchAddVods();
    void dropTables();
    void fetchThumbnail(qint64 rowid, bool download);
    int deleteVodsWhere(const QString& where);

private:
    mutable QMutex m_Lock;
    mutable QMutex m_AddQueueLock;
    QList<ScRecord> m_AddQueue;
    QThread m_AddThread;
    ScVodman* m_Vodman;
    QNetworkAccessManager* m_Manager;
    QSqlDatabase m_Database;
    Error m_Error;
    QHash<QNetworkReply*, ThumbnailRequest> m_ThumbnailRequests;
    QHash<QNetworkReply*, IconRequest> m_IconRequests;
    QHash<qint64, VodmanMetaDataRequest> m_VodmanMetaDataRequests;
    QHash<qint64, VodmanFileRequest> m_VodmanFileRequests;
    QNetworkReply* m_ClassfierRequest;
    ScIcons m_Icons;
    ScClassifier m_Classifier;
    QString m_ThumbnailDir;
    QString m_MetaDataDir;
    QString m_VodDir;
    QString m_IconDir;
    int m_SuspendedVodsChangedEventCount;
    bool m_Ready;
};

