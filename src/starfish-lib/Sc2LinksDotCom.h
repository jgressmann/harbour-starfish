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
    virtual void _fetch() Q_DECL_OVERRIDE;
    virtual void _cancel() Q_DECL_OVERRIDE;
    virtual QString _id() const Q_DECL_OVERRIDE;

private slots:
    void requestFinished(QNetworkReply* reply);

private:
    void parseLevel0(QNetworkReply* reply);
    void parseLevel1(QNetworkReply* reply);
    void parseLevel2(QNetworkReply* reply);
    QNetworkReply* makeRequest(const QUrl& url) const;
    void updateVodFetchingProgress();
    void pruneInvalidSeries();
    void saveRecords();
    static void toRecord(const ScEventData& event, const ScStageData& stage, const ScMatchData& match, ScRecord* record);
    void finish();


private:
    QList<ScRecord> m_Vods;
    QList<ScEvent> m_Events;
    QList<ScEvent> m_EventRequestQueue;
    QHash<QNetworkReply*, int> m_PendingRequests;
    QHash<QNetworkReply*, ScEvent> m_RequestStage;
    QHash<QNetworkReply*, ScStage> m_RequestMatch;
    QHash<QNetworkReply*, ScMatch> m_RequestVod;
    qreal m_VodFetchingProgress;
    int m_TotalUrlsToFetched;
    int m_CurrentUrlsToFetched;
};

