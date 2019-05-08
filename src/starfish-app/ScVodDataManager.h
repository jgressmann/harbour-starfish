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

#include "VMPlaylist.h"
#include "ScRecord.h"
#include "ScIcons.h"
#include "ScClassifier.h"
#include "ScMatchItem.h"
#include "ScUrlShareItem.h"
#include "ScVodDataManagerWorker.h" // for typedef ScVodIdList


#include <QSqlDatabase>
#include <QThread>
#include <QVariant>
#include <QSharedPointer>
#include <QTimer>

class QNetworkConfigurationManager;
class QNetworkAccessManager;
class QNetworkRequest;
class QNetworkReply;
class QUrl;
class ScDatabaseStoreQueue;
class ScVodman;
class ScEvent;
class ScStage;
class ScMatch;
class ScVodDataManagerState;
class ScVodDataManagerWorker;
class VMVodFileDownload;
class ScRecentlyWatchedVideos;
class ScMatchItem;


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
    Q_PROPERTY(ScRecentlyWatchedVideos* recentlyWatched READ recentlyWatched CONSTANT)
    Q_PROPERTY(bool isOnline READ isOnline NOTIFY isOnlineChanged)

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
    Q_ENUM(Error)

    enum State {
        State_Initializing,
        State_Ready,
        State_Error,
        State_Finalizing
    };
    Q_ENUM(State)


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
    Q_ENUM(ClearFlags)

    enum DataDirectoryChangeType {
        DDCT_StatCurrentDir,
        DDCT_StatTargetDir,
        DDCT_Copy,
        DDCT_Remove,
        DDCT_Move,
        DDCT_Finished,
    };
    Q_ENUM(DataDirectoryChangeType)


public:
    ~ScVodDataManager();
    explicit ScVodDataManager(QObject *parent = Q_NULLPTR);

public: //
    Q_INVOKABLE QString label(const QString& key, const QVariant& value = QVariant()) const;
    Q_INVOKABLE QString icon(const QString& key, const QVariant& value = QVariant()) const;
    bool ready() const;
    Error error() const { return m_Error; }
    inline QSqlDatabase database() const { return m_Database; }
    int fetchThumbnail(qint64 urlShareId, bool download);
    int fetchMetaData(qint64 urlShareId, const QString& url, bool download);
    int fetchTitle(qint64 urlShareId);
    int fetchVod(qint64 urlShareId, const QString& format);
    int cancelFetchVod(qint64 urlShareId);
    void cancelFetchMetaData(int ticket, qint64 urlShareId);
    void cancelFetchThumbnail(int ticket, qint64 urlShareId);
    int queryVodFiles(qint64 urlShareId);
    void deleteVodFiles(qint64 urlShareId);
    void deleteMetaData(qint64 urlShareId);
    Q_INVOKABLE void fetchIcons();
    Q_INVOKABLE void fetchClassifier();
    Q_INVOKABLE void resetDownloadMarker();
    QDate downloadMarker() const;
    QString downloadMarkerString() const;
    void setDownloadMarker(QDate value);
    void suspendVodsChangedEvents(int count = 1);
    void resumeVodsChangedEvents();
    // filter pages
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
    bool cancelTicket(int ticket);
    QString dataDirectory() const;
    int sqlPatchLevel() const;
    inline int maxConcurrentMetaDataDownloads() const { return m_MaxConcurrentMetaDataDownloads; }
    void setMaxConcurrentMetaDataDownloads(int value);
    Q_INVOKABLE int fetchSeen(qint64 rowid, const QString& table, const QString& where);
    Q_INVOKABLE int fetchVodEnd(qint64 rowid, int startOffsetS, int vodLengthS);
    // filter pages
    Q_INVOKABLE void deleteVod(qint64 rowid);
    Q_INVOKABLE int deleteVods(const QString& where);
    void deleteThumbnail(qint64 urlShareId);
    Q_INVOKABLE void setYtdlPath(const QString&  path);
    Q_INVOKABLE QString sqlEscapeLiteral(QString value);
    ScDatabaseStoreQueue* databaseStoreQueue() const;
    ScRecentlyWatchedVideos* recentlyWatched() const { return m_RecentlyWatchedVideos; }
    Q_INVOKABLE ScMatchItem* acquireMatchItem(qint64 rowid);
    Q_INVOKABLE void releaseMatchItem(ScMatchItem* item);
    bool isOnline() const;
    Q_INVOKABLE void clearYtdlCache();

signals:
    void readyChanged();
    void errorChanged();
    void vodsChanged();
    void downloadFailed(QString url, int error, QString filePath);
    void downloadMarkerChanged();
    void busyChanged();
    void vodsToAdd();
    void vodsAdded(int count);
    void vodDownloadsChanged();
    void sqlPatchLevelChanged();
    void dataDirectoryChanged();
    void dataDirectoryChanging(int changeType, QString path, float progress, int error, QString errorDescription);
    void startWorker();
    void stopThread();
    void maxConcurrentMetaDataDownloadsChanged(int value);
    void startProcessDatabaseStoreQueue(int transactionId, QString sql, QVariantList args);
    void processVodsToAdd();
    void ytdlPathChanged(QString path);
    void seenChanged();
    void isOnlineChanged();


public slots:
    void excludeEvent(const ScEvent& event, bool* exclude);
    void excludeStage(const ScStage& stage, bool* exclude);
    void excludeMatch(const ScMatch& match, bool* exclude);
    void hasRecord(const ScRecord& record, bool* exclude);
    void addVods(QList<ScRecord> vods);

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

    struct MatchItemData
    {
        alignas(std::alignment_of<ScMatchItem>::value) unsigned char Raw[sizeof(ScMatchItem)];
        int RefCount;

        MatchItemData() : Raw{}, RefCount(0) {}
        const ScMatchItem* matchItem() const { return reinterpret_cast<const ScMatchItem*>(Raw); }
        ScMatchItem* matchItem() { return reinterpret_cast<ScMatchItem*>(Raw); }
    };

    using DatabaseCallback = std::function<void(qint64 insertIdOrNumRows, bool error)>;

private slots:
    void requestFinished(QNetworkReply* reply);
    void onDataDirectoryChanging(int changeType, QString path, float progress, int error, QString errorDescription);
    void onVodDownloadsChanged(ScVodIdList ids);
    void databaseStoreCompleted(int token, qint64 insertIdOrNumRowsAffected, int error, QString errorDescription);
    void addVodFromQueue();
    void onUrlShareItemDestroyed(QObject* obj);
    void onFetchingMetaData(qint64 urlShareId);
    void onFetchingThumbnail(qint64 urlShareId);
    void onMetaDataAvailable(qint64 urlShareId, VMPlaylist playlist);
    void onMetaDataUnavailable(qint64 urlShareId);
    void onMetaDataDownloadFailed(qint64 urlShareId, VMVodEnums::Error error);
    void onVodAvailable(const ScVodFileFetchProgress& progress);
    void onFetchingVod(const ScVodFileFetchProgress& progress);
    void onVodUnavailable(qint64 urlShareId);
    void onVodDownloadFailed(qint64 urlShareId, VMVodEnums::Error error);
    void onVodDownloadCanceled(qint64 urlShareId);
    void onThumbnailAvailable(qint64 urlShareId, QString filePath);
    void onThumbnailUnavailable(qint64 urlShareId, ScVodDataManagerWorker::ThumbnailError error);
    void onThumbnailDownloadFailed(qint64 urlShareId, int error, QString url);
    void onTitleAvailable(qint64 urlShareId, QString title);
    void onSeenAvailable(qint64 rowid, qreal seen);
    void onVodEndAvailable(qint64 rowid, int endOffsetS);
private:
    static void clearMatchItems(QHash<qint64, MatchItemData*>& h);
    static bool tryGetEvent(QString& inoutSrc, QString* location);
    static bool tryGetIcon(const QString& title, QString* iconPath);
    void setupDatabase();
    void setError(Error value);
    void updateStatus();
    void setStatusFromRowCount();
    void clearVods();
    static void parseUrl(const QString& url,
                         QString* cannonicalUrl,
                         QString* outId, int* outUrlType, int* outStartOffset);
    static int parseTwitchOffset(const QString& str);
    void tryRaiseVodsChanged();
    void getSeries(const QString& str, QString& series, QString& event);
    bool exists(
            const ScEvent& event,
            const ScStage& stage,
            const ScMatch& match,
            bool* exists) const;

    bool exists(QSqlQuery& query, const ScRecord& record, qint64* id) const;


    void iconRequestFinished(QNetworkReply* reply, IconRequest& r);
    void insertVod(const ScRecord& record, qint64 urlShareId, int startOffset);
    void dropTables();
    int deleteVodFilesWhere(int transactionId, const QString& where, bool raiseChanged);
    int deleteThumbnailsWhere(int transactionId, const QString& where);
    void fetchSqlPatches();
    void applySqlPatches(const QByteArray& bytes);
    void setPersistedValue(QSqlQuery& query, const QString& key, const QString& value);
    void setState(State state);
    void setDataDirectory(const QString& value);
    void setDirectories();
    void pruneExpiredMatchItems();


    void updateSql1(QSqlQuery& q);
    void updateSql2(QSqlQuery& q);
    void updateSql3(QSqlQuery& q);
    void updateSql4(QSqlQuery& q);
    void updateSql5(QSqlQuery& q, const char*const* createSql, size_t createSqlCount);
    void updateSql6(QSqlQuery& q, const char*const* createSql, size_t createSqlCount);
    void updateSql7(QSqlQuery& q, const char*const* createSql, size_t createSqlCount);


private:
    QTimer m_MatchItemTimer;
    QList<ScRecord> m_AddQueue;
    QThread m_WorkerThread;
    QThread m_DatabaseStoreQueueThread;
    QNetworkAccessManager* m_Manager;
    QNetworkConfigurationManager* m_NetworkConfigurationManager;
    ScVodDataManagerWorker* m_WorkerInterface;
    QSqlDatabase m_Database;
    Error m_Error;
    QHash<QNetworkReply*, IconRequest> m_IconRequests;
    QNetworkReply* m_ClassfierRequest;
    QNetworkReply* m_SqlPatchesRequest;
    DataDirectoryMover* m_DataDirectoryMover;
    ScRecentlyWatchedVideos* m_RecentlyWatchedVideos;
    ScIcons m_Icons;
    ScClassifier m_Classifier;
    int m_SuspendedVodsChangedEventCount;
    int m_MaxConcurrentMetaDataDownloads;
    int m_AddCounter;
    QAtomicInt m_State;
    QSharedPointer<ScVodDataManagerState> m_SharedState;
    QVariantList m_VodDownloads;
    QHash<int, DatabaseCallback> m_PendingDatabaseStores;
    QHash<qint64, QWeakPointer<ScUrlShareItem>> m_UrlShareItems;
    QHash<QObject*, qint64> m_UrlShareItemToId;
    QHash<qint64, MatchItemData*> m_ActiveMatchItems;
    QHash<qint64, MatchItemData*> m_ExpiredMatchItems;
};

