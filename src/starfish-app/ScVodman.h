#pragma once

#include "service_interface.h" // http://inz.fi/2011/02/18/qmake-and-d-bus/
#include <vodman/VMVod.h>
#include <QMutex>
#include <QAtomicInt>

class QByteArray;
class QDBusPendingCallWatcher;
class VMVod;
class VMVodFileDownload;

class ScVodman : public QObject
{
    Q_OBJECT
    Q_PROPERTY(int maxConcurrentMetaDataDownloads READ maxConcurrentMetaDataDownloads WRITE setMaxConcurrentMetaDataDownloads NOTIFY maxConcurrentMetaDataDownloadsChanged)
public:
    ~ScVodman();
    explicit ScVodman(QObject* parent = Q_NULLPTR);

    qint64 newToken();
    void startFetchMetaData(qint64 token, const QString& url);
    void startFetchFile(qint64 token, const VMVod& vod, int formatIndex, const QString& filePath);
    void cancel(qint64 token);
    void cancel();
    int maxConcurrentMetaDataDownloads() const { return m_MaxMeta; }
    void setMaxConcurrentMetaDataDownloads(int value);


signals:
    void metaDataDownloadCompleted(qint64 token, const VMVod& vod);
    void fileDownloadChanged(qint64 token, const VMVodFileDownload& download);
    void fileDownloadCompleted(qint64 token, const VMVodFileDownload& download);
    void downloadFailed(qint64 token, int serviceErrorCode);
    void maxConcurrentMetaDataDownloadsChanged();

private slots:
    void onVodFileDownloadRemoved(qint64 handle, const QByteArray& download);
    void onVodFileDownloadChanged(qint64 handle, const QByteArray& download);
    void onVodFileMetaDataDownloadCompleted(qint64 handle, const QByteArray& download);
    void onNewTokenReply(QDBusPendingCallWatcher *self);
    void onMetaDataDownloadReply(QDBusPendingCallWatcher *self);
    void onFileDownloadReply(QDBusPendingCallWatcher *self);



private:
    enum RequestType {
        RT_MetaData,
        RT_File,
    };

    struct Request {
        RequestType type;
        qint64 token;
        QString url;
        QString filePath;
        VMVod vod;
        int formatIndex;
    };

private:
    void scheduleNextFileRequest();
    void scheduleNextMetaDataRequest();
    void issueRequest(qint64 token, const Request& request);

private:
    QMutex m_Lock;
    org::duckdns::jgressmann::vodman::service* m_Service;
    QList<QPair<qint64, Request>> m_PendingFileRequests;
    QList<QPair<qint64, Request>> m_PendingMetaDataRequests;
    QHash<qint64, Request> m_ActiveRequests;
    QHash<QDBusPendingCallWatcher*, int> m_PendingDBusResponses;
    QHash<qint64, qint64> m_MetaDataTokenMap;
    QHash<qint64, qint64> m_FileTokenMap;
    QAtomicInteger<qint64> m_TokenGenerator;
    int m_MaxFile, m_CurrentFile, m_MaxMeta, m_CurrentMeta;
};
