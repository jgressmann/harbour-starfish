#pragma once

#include <vodman/VMVod.h>
#include "ScRecord.h"
#include "ScIcons.h"

#include <QMutex>
#include <QVariant>
#include <QSqlDatabase>

class QNetworkAccessManager;
class QNetworkRequest;
class QNetworkReply;
class QUrl;
class ScVodman;
class ScEvent;
class ScStage;
class ScMatch;
class VMVodFileDownload;
class ScVodDataManager : public QObject {

    Q_OBJECT
    Q_PROPERTY(Status status READ status NOTIFY statusChanged)
    Q_PROPERTY(Error error READ error NOTIFY errorChanged)
    Q_PROPERTY(ScVodman* vodman READ vodman CONSTANT)
public:


    enum Status {
        Status_Ready,
        Status_VodFetchingInProgress,
        Status_VodFetchingComplete,
        Status_VodFetchingBeingCanceled,
        Status_Error,
    };
    Q_ENUMS(Status)

    enum Error {
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
    Status status() const { return m_Status; }
    Error error() const { return m_Error; }
    QSqlDatabase database() const { return m_Database; }
    Q_INVOKABLE void fetchMetaData(qint64 rowid);
    Q_INVOKABLE void fetchMetaDataFromCache(qint64 rowid);
    Q_INVOKABLE void fetchThumbnail(qint64 rowid);
    Q_INVOKABLE QString title(qint64 rowid);
    Q_INVOKABLE void fetchVod(qint64 rowid, int formatIndex);
    Q_INVOKABLE void cancelFetchVod(qint64 rowid);
    Q_INVOKABLE void cancelFetchMetaData(qint64 rowid);
    Q_INVOKABLE void queryVodFiles(qint64 rowid);
    Q_INVOKABLE void deleteVod(qint64 rowid);
    Q_INVOKABLE void deleteMetaData(qint64 rowid);
    void addVods(const QList<ScRecord>& records);
    QDate downloadMarker() const;
    void setDownloadMarker(QDate value);
    void suspendVodsChangedEvents();
    void resumeVodsChangedEvents();
    Q_INVOKABLE qreal seen(const QVariantMap& filters) const;
    Q_INVOKABLE void setSeen(const QVariantMap& filters, bool value);
    ScVodman* vodman() const { return m_Vodman; }

signals:
    void statusChanged();
    void errorChanged();
    void vodsChanged();
    void metaDataAvailable(qint64 rowid, VMVod vod);
    void vodAvailable(qint64 rowid, QString filePath, qreal progress, quint64 fileSize, int width, int height, QString formatId);
    void thumbnailAvailable(qint64 rowid, QString filePath);
    void thumbnailDownloadFailed(qint64 rowid, int error, QString url);
    void downloadFailed(QString url, int error, QString filePath);

public slots:
    void excludeEvent(const ScEvent& event, bool* exclude);
    void excludeStage(const ScStage& stage, bool* exclude);
    void excludeMatch(const ScMatch& match, bool* exclude);
    void hasRecord(const ScRecord& record, bool* exclude);
    void clear();
    void vacuum();

private slots:
    void onMetaDataDownloadCompleted(qint64 token, const VMVod& vod);
    void onFileDownloadChanged(qint64 token, const VMVodFileDownload& download);
    void onFileDownloadCompleted(qint64 token, const VMVodFileDownload& download);
    void onDownloadFailed(qint64 token, int serviceErrorCode);
    void requestFinished(QNetworkReply* reply);

private:
    static bool tryGetEvent(QString& inoutSrc, QString* location);
    static bool tryGetIcon(const QString& title, QString* iconPath);
    void createTablesIfNecessary();
    void setStatus(Status value);
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
    bool _addVods(const QList<ScEvent>& events);
    bool _addVods(const QList<ScRecord>& records);
    void tryRaiseVodsChanged();
    void getSeries(const QString& str, QString& series, QString& event);
    bool exists(
            const ScEvent& event,
            const ScStage& stage,
            const ScMatch& match,
            bool* exists) const;

    bool exists(const ScRecord& record, bool* exists) const;
    void updateVodDownloadStatus(qint64 vodFileId, const VMVodFileDownload& download);
    void fetchMetaData(qint64 rowid, bool download);

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
        int formatIndex;
        qint64 token;
        qint64 vod_url_share_id;
        qint64 vod_file_id;
    };

    struct ThumbnailRequest {
        qint64 rowid;
    };

    struct IconRequest {
        QString url;
    };

private:
    mutable QMutex m_Lock;
    ScVodman* m_Vodman;
    QNetworkAccessManager* m_Manager;
    QSqlDatabase m_Database;
    Error m_Error;
    Status m_Status;
    QHash<QNetworkReply*, ThumbnailRequest> m_ThumbnailRequests;
    QHash<QNetworkReply*, IconRequest> m_IconRequests;
    QHash<qint64, VodmanMetaDataRequest> m_VodmanMetaDataRequests;
    QHash<qint64, VodmanFileRequest> m_VodmanFileRequests;
    ScIcons m_Icons;
    QString m_ThumbnailDir;
    QString m_MetaDataDir;
    QString m_VodDir;
    QString m_IconDir;
    int m_SuspendedVodsChangedEventCount;

};

