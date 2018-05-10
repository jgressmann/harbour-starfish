#pragma once

#include "ScVodScraper.h"
#include "ScRecord.h"


class QDateTime;
class QNetworkRequest;
class QNetworkReply;
class QUrl;

class Sc2CastsDotCom : public ScVodScraper {

    Q_OBJECT
public:
    explicit Sc2CastsDotCom(QObject *parent = Q_NULLPTR);

public: //
    virtual QList<ScRecord> vods() const { return m_Vods; }

protected:
    virtual void _fetch();
    virtual void _cancel();

private slots:
    void requestFinished(QNetworkReply* reply);

private:
    void parseLevel0(QNetworkReply* reply);
    void parseLevel1(QNetworkReply* reply);
    QNetworkReply* makeRequest(const QUrl& url) const;
    void updateVodFetchingProgress();
    QString makePageUrl(int page) const;
    void pruneInvalidVods();

private:
    QList<ScRecord> m_Vods;
    QHash<QNetworkReply*, int> m_PendingRequests;
    QHash<QNetworkReply*, int> m_ReplyToRecordTable;
    qreal m_VodFetchingProgress;
    int m_UrlsToFetch;
    int m_UrlsFetched;
    int m_CurrentPage;
};

