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
#include "ScVodDataManagerWorker.h" // for typedef ScVodIdList


#include <QMutex>
#include <QSqlDatabase>
#include <QThread>
#include <QVariant>
#include <QSharedPointer>


class QNetworkAccessManager;
class QNetworkRequest;
class QNetworkReply;
class QUrl;
class ScVodman;
class ScEvent;
class ScStage;
class ScMatch;
class ScVodDataManagerState;
class ScVodDataManagerWorker;
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


class DataDirectoryMover : public QObject
{
    Q_OBJECT
public:
    ~DataDirectoryMover() = default;
    DataDirectoryMover() = default;
signals:
    void dataDirectoryChanging(int changeType, QString path, float progress, int error, QString errorDescription);
public slots:
    void move(const QString& currentDir, const QString& targetDir);
    void cancel();
    void onProcessReadyRead();

private:
    void _move(const QString& currentDir, QString targetDir);
private:
    QAtomicInt m_Canceled;
};




class ScVodDataManager : public QObject {

    Q_OBJECT
    Q_PROPERTY(Error error READ error NOTIFY errorChanged)
    Q_PROPERTY(int maxConcurrentMetaDataDownloads READ maxConcurrentMetaDataDownloads WRITE setMaxConcurrentMetaDataDownloads NOTIFY maxConcurrentMetaDataDownloadsChanged)
    Q_PROPERTY(QString downloadMarker READ downloadMarkerString NOTIFY downloadMarkerChanged)
    Q_PROPERTY(bool busy READ busy NOTIFY busyChanged)
    Q_PROPERTY(bool ready READ ready NOTIFY readyChanged)
    Q_PROPERTY(QVariant database READ databaseVariant CONSTANT)
    Q_PROPERTY(int vodDownloads READ vodDownloads NOTIFY vodDownloadsChanged)
    Q_PROPERTY(int sqlPatchLevel READ sqlPatchLevel NOTIFY sqlPatchLevelChanged)
    Q_PROPERTY(QString dataDirectory READ dataDirectory NOTIFY dataDirectoryChanged)

public:
    enum Error {
        Error_Unknown = -1,
        Error_None,
        Error_CouldntCreateSqlTables,
        Error_CouldntStartTransaction,
        Error_CouldntEndTransaction,
        Error_SqlTableManipError,
        Error_ProcessOutOfResources,
        Error_ProcessTimeout,
        Error_ProcessCrashed,
        Error_ProcessFailed,
        Error_RenameFailed,
        Error_Canceled,
        Error_Warning,
        Error_StatFailed,
        Error_NoSpaceLeftOnDevice,
    };
    Q_ENUMS(Error)

    enum State {
        State_Initializing,
        State_Ready,
        State_Error,
        State_Finalizing
    };
    Q_ENUMS(State)


    enum ClearFlags {
        CF_MetaData = 0x01,
        CF_Thumbnails = 0x02,
        CF_Vods = 0x04,
        CF_Icons = 0x08,
        CF_Everthing =
            CF_MetaData |
            CF_Thumbnails |
            CF_Vods |
            CF_Icons
    };
    Q_ENUMS(ClearFlags)

    enum DataDirectoryChangeType {
        DDCT_StatCurrentDir,
        DDCT_StatTargetDir,
        DDCT_Copy,
        DDCT_Remove,
        DDCT_Move,
        DDCT_Finished,
    };
    Q_ENUMS(DataDirectoryChangeType)

public:
    ~ScVodDataManager();
    explicit ScVodDataManager(QObject *parent = Q_NULLPTR);

public: //
    Q_INVOKABLE QString label(const QString& key, const QVariant& value = QVariant()) const;
    Q_INVOKABLE QString icon(const QString& key, const QVariant& value = QVariant()) const;
    bool ready() const;
    Error error() const { return m_Error; }
    inline QSqlDatabase database() const { return m_Database; }
    Q_INVOKABLE int fetchMetaData(qint64 rowid);
    Q_INVOKABLE int fetchMetaDataFromCache(qint64 rowid);
    Q_INVOKABLE int fetchThumbnail(qint64 rowid);
    Q_INVOKABLE int fetchThumbnailFromCache(qint64 rowid);
    Q_INVOKABLE int fetchTitle(qint64 rowid);
    Q_INVOKABLE int fetchVod(qint64 rowid, int formatIndex);
    Q_INVOKABLE void cancelFetchVod(qint64 rowid);
    Q_INVOKABLE void cancelFetchMetaData(int ticket, qint64 rowid);
    Q_INVOKABLE void cancelFetchThumbnail(int ticket, qint64 rowid);
    Q_INVOKABLE int queryVodFiles(qint64 rowid);
    Q_INVOKABLE void deleteVodFiles(qint64 rowid);
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
    ScClassifier* classifier() const;
    QVariant databaseVariant() const;
    bool busy() const;
    Q_INVOKABLE QString makeThumbnailFile(const QString& srcPath);
    Q_INVOKABLE int deleteSeenVodFiles(const QString& where);
    int vodDownloads() const;
    Q_INVOKABLE QVariantList vodsBeingDownloaded() const;
    Q_INVOKABLE QString getPersistedValue(const QString& key, const QString& defaultValue = QString()) const;
    Q_INVOKABLE void setPersistedValue(const QString& key, const QString& value);
    Q_INVOKABLE void resetSqlPatchLevel();
    Q_INVOKABLE QString raceIcon(int race) const;
    Q_INVOKABLE int vodEndOffset(qint64 rowid, int startOffsetS, int vodLengthS) const;
    Q_INVOKABLE void clear();
    Q_INVOKABLE void clearCache(ClearFlags flags);
    Q_INVOKABLE void moveDataDirectory(const QString& targetDirectory);
    Q_INVOKABLE void cancelMoveDataDirectory();
    Q_INVOKABLE bool cancelTicket(int ticket);
    QString dataDirectory() const;
    int sqlPatchLevel() const;
    inline int maxConcurrentMetaDataDownloads() const { return m_MaxConcurrentMetaDataDownloads; }
    void setMaxConcurrentMetaDataDownloads(int value);
    Q_INVOKABLE int fetchSeen(qint64 rowid, const QString& table, const QString& where);
    Q_INVOKABLE int fetchVodEnd(qint64 rowid, int startOffsetS, int vodLengthS);
    Q_INVOKABLE void deleteVod(qint64 rowid);
    Q_INVOKABLE void deleteThumbnail(qint64 rowid);

signals:
    void readyChanged();
    void errorChanged();
    void vodsChanged();
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
    void downloadFailed(QString url, int error, QString filePath);
    void downloadMarkerChanged();
    void busyChanged();
    void vodsToAdd();
    void vodsCleared();
    void vodmanError(int error);
    void vodDownloadFailed(qint64 rowid, int error);
    void vodDownloadCanceled(qint64 rowid);
    void vodsAdded(int count);
    void vodDownloadsChanged();
    void sqlPatchLevelChanged();
    void dataDirectoryChanged();
    void dataDirectoryChanging(int changeType, QString path, float progress, int error, QString errorDescription);
    void startWorker();
    void stopWorker();
    void maxConcurrentMetaDataDownloadsChanged(int value);
    void titleAvailable(qint64 rowid, QString title);
    void seenAvailable(qint64 rowid, qreal seen);
    void vodEndAvailable(qint64 rowid, int endOffsetS);

public slots:
    void excludeEvent(const ScEvent& event, bool* exclude);
    void excludeStage(const ScStage& stage, bool* exclude);
    void excludeMatch(const ScMatch& match, bool* exclude);
    void hasRecord(const ScRecord& record, bool* exclude);

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

    struct IconRequest {
        QString url;
    };


private slots:
    void requestFinished(QNetworkReply* reply);
    void vodAddWorkerFinished();
    void onDataDirectoryChanging(int changeType, QString path, float progress, int error, QString errorDescription);
    void onVodDownloadsChanged(ScVodIdList ids);

private:
    static bool tryGetEvent(QString& inoutSrc, QString* location);
    static bool tryGetIcon(const QString& title, QString* iconPath);
    void setupDatabase();
    void setError(Error value);
    void updateStatus();
    void setStatusFromRowCount();
    void clearVods();

    void fetchMetaData(qint64 urlShareId, const QString& url);
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

    int fetchMetaData(qint64 rowid, bool download);
    void iconRequestFinished(QNetworkReply* reply, IconRequest& r);

    static void batchAddVods(void* ctx);
    void batchAddVods();
    void dropTables();
    int fetchThumbnail(qint64 rowid, bool download);
    int deleteVodFilesWhere(const QString& where);
    void fetchSqlPatches();
    void applySqlPatches(const QByteArray& bytes);
    void setPersistedValue(QSqlQuery& query, const QString& key, const QString& value);
    void setState(State state);
    void setDataDirectory(const QString& value);
    void setDirectories();
    void updateSql1(QSqlQuery& q);
    void updateSql2(QSqlQuery& q);
    void updateSql3(QSqlQuery& q);
    void updateSql4(QSqlQuery& q);
    void updateSql5(QSqlQuery& q, const char*const* createSql, size_t createSqlCount);

private:
    mutable QMutex m_Lock;
    mutable QMutex m_AddQueueLock;
    QList<ScRecord> m_AddQueue;
    QThread m_AddThread;
    QThread m_WorkerThread;
    QNetworkAccessManager* m_Manager;
    ScVodDataManagerWorker* m_WorkerInterface;
    QSqlDatabase m_Database;
    Error m_Error;
    QHash<QNetworkReply*, IconRequest> m_IconRequests;
    QNetworkReply* m_ClassfierRequest;
    QNetworkReply* m_SqlPatchesRequest;
    DataDirectoryMover* m_DataDirectoryMover;
    ScIcons m_Icons;
    ScClassifier m_Classifier;
    int m_SuspendedVodsChangedEventCount;
    int m_MaxConcurrentMetaDataDownloads;
    QAtomicInt m_State;
    QSharedPointer<ScVodDataManagerState> m_SharedState;
    QVariantList m_VodDownloads;
};

