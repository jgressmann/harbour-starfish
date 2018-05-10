#pragma once

#include "ScVodScraper.h"


class QDateTime;
class QNetworkRequest;
class QNetworkReply;
class QUrl;

class Sc2LinksDotCom : public ScVodScraper {

    Q_OBJECT
public:
    explicit Sc2LinksDotCom(QObject *parent = Q_NULLPTR);

public: //
    virtual QList<ScRecord> vods() const { return m_Vods; }
    virtual bool canSkip() const Q_DECL_OVERRIDE;

protected:
    virtual void _fetch();
    virtual void _cancel();

private slots:
    void requestFinished(QNetworkReply* reply);

private:
    void parseLevel0(QNetworkReply* reply);
    void parseLevel1(QNetworkReply* reply);
    void parseLevel2(QNetworkReply* reply);
    QNetworkReply* makeRequest(const QUrl& url) const;
    void updateVodFetchingProgress();
    void pruneInvalidSeries();

private:
    QList<ScRecord> m_Vods;
    QList<ScEvent> m_Vods2;
    QList<ScEvent> m_EventRequestQueue;
    QHash<QNetworkReply*, int> m_PendingRequests;
    QHash<QNetworkReply*, ScEvent> m_RequestStage;
    QHash<QNetworkReply*, ScStage> m_RequestMatch;
    QHash<QNetworkReply*, ScMatch> m_RequestVod;
    qreal m_VodFetchingProgress;
    int m_TotalUrlsToFetched;
    int m_CurrentUrlsToFetched;
};

