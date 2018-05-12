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

#include <QNetworkRequest>
#include <QNetworkReply>
#include <QRegExp>
#include <QMutexLocker>
#include <QUrl>
#include <QDateTime>

#include "Sc2LinksDotCom.h"
#include "ScRecord.h"
#include "Sc.h"


namespace  {

const QString EntryUrl = QStringLiteral("https://www.sc2links.com/vods/");


//                                      <a href="      https://www.sc2links.com/tournament/?match=502"           >WCS Montreal</a></div><div    class="    voddate"         >September 11th 2017</div></br>
const QRegExp aHrefRegex(QStringLiteral("<a href=['\"](https://www.sc2links.com/tournament/\\?match=\\d+)['\"][^>]*>([^>]+)</a></div><div\\s+class=['\"]voddate['\"][^>]*>([^>]+)</div>"));
const QRegExp tournamentPageStageNameRegex(QStringLiteral("<h3>([^>]+)</h3>\\s*<h5>"));
const QRegExp tournamentPageStageContent(QStringLiteral("<h3>([^>]+)</h3>\\s*(<h5>.*)<h"));
const QRegExp tournamentPageMatchRegex(QStringLiteral("<h5>.*<a\\s+href\\s*=\\s*['\"](https://www.sc2links.com/match/\\?match=\\d+)['\"][^>]*>.*<div\\s+class\\s*=\\s*['\"]match['\"]\\s*>(\\w+)\\s+(\\d+)</div>\\s*</a>(.*)<div\\s+class\\s*=\\s*['\"]date['\"]\\s*>([^>]+)</div>.*</h5>"));
const QRegExp iFrameRegex(QStringLiteral("<iframe\\s+(?:[^>]+\\s+)*src=['\"]([^'\"]+)['\"][^>]*>"));

const QRegExp dateRegex(QStringLiteral("\\d{4}-\\d{2}-\\d{2}"));
const QRegExp fuzzyYearRegex(QStringLiteral(".*(\\d{4}).*"));
const QRegExp yearRegex(QStringLiteral("\\d{4}"));
const QRegExp tags(QStringLiteral("<[^>]+>"));
const QRegExp englishMonthRegex(QStringLiteral("january|febuary|march|april|may|june|july|august|september|november|december"), Qt::CaseInsensitive);

int InitializeStatics() {
    Q_ASSERT(aHrefRegex.isValid());
    Q_ASSERT(dateRegex.isValid());
    Q_ASSERT(yearRegex.isValid());
    Q_ASSERT(tags.isValid());
    Q_ASSERT(tournamentPageStageNameRegex.isValid());
    Q_ASSERT(tournamentPageMatchRegex.isValid());
    Q_ASSERT(iFrameRegex.isValid());

    const_cast<QRegExp&>(tags).setMinimal(true);
    const_cast<QRegExp&>(yearRegex).setMinimal(true);
    const_cast<QRegExp&>(tournamentPageMatchRegex).setMinimal(true);

    return 1;
}

int s_StaticsInitialized = InitializeStatics();

} // anon



Sc2LinksDotCom::Sc2LinksDotCom(QObject *parent)
    : ScVodScraper(parent)
{
    m_TotalUrlsToFetched = 0;
    m_CurrentUrlsToFetched = 0;
    (void)s_StaticsInitialized;

    connect(networkAccessManager(), SIGNAL(finished(QNetworkReply*)), this, SLOT(requestFinished(QNetworkReply*)));
}

void
Sc2LinksDotCom::_fetch() {
    m_Vods.clear();
    QNetworkReply* reply = makeRequest(EntryUrl);
    m_PendingRequests.insert(reply, 0);
    m_TotalUrlsToFetched = 1;
    m_CurrentUrlsToFetched = 0;
    setProgressDescription(tr("Fetching list of events"));
    updateVodFetchingProgress();
    qDebug("fetch started");
}

QNetworkReply*
Sc2LinksDotCom::makeRequest(const QUrl& url) const {
    return networkAccessManager()->get(Sc::makeRequest(url));
}

void
Sc2LinksDotCom::requestFinished(QNetworkReply* reply) {
    QMutexLocker g(lock());
    reply->deleteLater();
    const int level = m_PendingRequests.value(reply, -1);
    m_PendingRequests.remove(reply);

    switch (status()) {
    case Status_VodFetchingInProgress:
        switch (reply->error()) {
        case QNetworkReply::OperationCanceledError:
            break;
        case QNetworkReply::NoError: {
            // Get the http status code
            int v = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
            if (v >= 200 && v < 300) // Success
            {
                // Here we got the final reply
                switch (level) {
                case 0:
                    parseLevel0(reply);
                    break;
                case 1:
                    parseLevel1(reply);
                    break;
                case 2:
                    parseLevel2(reply);
                    break;
                default:
                    qCritical() << "Unhandled level" << level;
                    break;
                }
            }
            else if (v >= 300 && v < 400) // Redirection
            {
                // Get the redirection url
                QUrl newUrl = reply->attribute(QNetworkRequest::RedirectionTargetAttribute).toUrl();
                // Because the redirection url can be relative,
                // we have to use the previous one to resolve it
                newUrl = reply->url().resolved(newUrl);

                m_PendingRequests.insert(makeRequest(newUrl), level);
                ++m_TotalUrlsToFetched;
            } else  {
                qDebug() << "Http status code:" << v;
            }
        } break;
        default:
            qDebug() << "Network request failed: " << reply->errorString();
            break;
        }

        m_RequestStage.remove(reply);
        m_RequestMatch.remove(reply);
        m_RequestVod.remove(reply);

        ++m_CurrentUrlsToFetched;
        updateVodFetchingProgress();


        if (m_PendingRequests.isEmpty()) {
            if (m_EventRequestQueue.empty()) {

                finish();
                setStatus(Status_VodFetchingComplete);

                setProgressDescription(QString());
                qDebug("fetch finished");
            } else {
                ScEvent event = m_EventRequestQueue.first();
                m_EventRequestQueue.pop_front();

                QNetworkReply* reply = makeRequest(event.url());
                m_RequestStage.insert(reply, event);
                m_PendingRequests.insert(reply, 1);
                ++m_TotalUrlsToFetched;
                setProgressDescription(tr("Fetching ") + event.name());

                qDebug() << "fetch event" << event.name() << event.year() << event.game() << event.season();
            }
        }
        break;
    case Status_VodFetchingBeingCanceled:
        if (m_PendingRequests.isEmpty()) {
            m_RequestStage.clear();
            m_RequestMatch.clear();
            m_RequestVod.clear();
            finish();
            setStatus(Status_VodFetchingCanceled);
        }
        break;
    default:
        if (m_PendingRequests.isEmpty()) {
            m_RequestStage.clear();
            m_RequestMatch.clear();
            m_RequestVod.clear();
            finish();
            setStatus(Status_Ready);
        }
        break;
    }
}

void
Sc2LinksDotCom::finish() {
    Q_ASSERT(m_RequestStage.empty());
    Q_ASSERT(m_RequestMatch.empty());
    Q_ASSERT(m_RequestVod.empty());

//    pruneInvalidSeries();
//    saveRecords();
    m_Events.clear();
}

void
Sc2LinksDotCom::parseLevel0(QNetworkReply* reply) {
    QString soup = QString::fromUtf8(reply->readAll());

    bool any = false;
    ScRecord record;
    for (int start = 0, x = 0, found = aHrefRegex.indexIn(soup, start);
         found != -1/* && x < 1*/;
         start = found + aHrefRegex.cap(0).length(), found = aHrefRegex.indexIn(soup, start), ++x) {

        any = true;

        QString link = aHrefRegex.cap(1);
        QString name = aHrefRegex.cap(2).trimmed();
        QString date = aHrefRegex.cap(3);

//        ScEnums::Game game = ScEnums::Game_Unknown;
        int year = 0;
        int season = 0;
        QString fullTitle = name;

        if (yearRegex.indexIn(date) >= 0) {
            year = yearRegex.cap(0).toInt();
        }

        ScRecord record;
        record.eventFullName = name;
        record.valid = ScRecord::ValidEventFullName;

        record.autoComplete(*classifier());

//        if (record.valid & ScRecord::ValidGame) {
//            switch (record.game) {
//            case ScRecord::GameBroodWar:
//                game = ScEnums::Game_Broodwar;
//                break;
//            case ScRecord::GameSc2:
//                game = ScEnums::Game_Sc2;
//                break;
//            default:
//                game = ScEnums::Game_Unknown;
//                break;
//            }
//        }

        if (record.valid & ScRecord::ValidSeason) {
            season = record.season;
        }

        if (record.valid & ScRecord::ValidYear) {
            year = record.year;
        }

        ScEvent ev;
        ScEventData& data = ev.data();

        data.fullName = fullTitle;
//        data.game = game;
        data.name = record.eventName;
        data.season = season;
        data.year = year;
        data.url = link;
        bool exclude = false;
        emit excludeEvent(ev, &exclude);

        if (exclude) {
            qDebug() << "exclude event" << name << year << season;
        } else {
            m_Events << ev;

            m_EventRequestQueue.append(ev);

            qDebug() << "fetch event" << name << year << season;
        }
    }

    if (!any) {
        qDebug("%s\n", qPrintable(soup));
    }
}

void
Sc2LinksDotCom::parseLevel1(QNetworkReply* reply) {
    QString soup = QString::fromUtf8(reply->readAll());
//    qDebug("%s\n", qPrintable(soup));

    QList< int > stageOffsets;
    QList< bool > stageExclude;
    QList<ScStage> stages;
    ScEvent& event = m_RequestStage[reply];
    ScEventData& eventData = event.data();

    //if (!event.name().startsWith("GSL Season One Code S 2017")) {
//    if (!event.name().startsWith("GSL")) {
//        return;
//    }


    for (int stageStart = 0, stageFound = tournamentPageStageNameRegex.indexIn(soup, stageStart), stageIndex = 0;
         stageFound != -1;
         stageStart = stageFound + tournamentPageStageNameRegex.cap(0).length(), stageFound = tournamentPageStageNameRegex.indexIn(soup, stageStart), ++stageIndex) {

        QString name = tournamentPageStageNameRegex.cap(1);

        ScStage stage;
        ScStageData& stageData = stage.data();
        stageData.name = name;
        stageData.url = reply->url().toString();
        stageData.owner = event.toWeak();
        bool exclude = false;
        emit excludeStage(stage, &exclude);

        stages << stage;
        stageExclude << exclude;
        stageOffsets << stageFound;
    }

    stageOffsets << soup.length();
    ScRecord record;

    for (int i = 0; i < stageOffsets.size() - 1; ++i) {

        ScStage& stage = stages[i];
        ScStageData& stageData = stage.data();
        stageData.stageNumber = stageOffsets.size() - i - 1;

        if (stageExclude[i]) {
            qDebug() << "exclude stage" << stageData.name;
            continue;
        }

        qDebug() << "parse stage" << stageData.name;

        QStringRef subString(&soup, stageOffsets[i], stageOffsets[i+1]-stageOffsets[i]);
        QString matchPart = subString.toString();
//        qDebug("%s\n", qPrintable(matchPart));

        for (int matchStart = 0, matchFound = tournamentPageMatchRegex.indexIn(matchPart, matchStart);
             matchFound != -1;
             matchStart = matchFound + tournamentPageMatchRegex.cap(0).length(), matchFound = tournamentPageMatchRegex.indexIn(matchPart, matchStart)) {

            QString link = tournamentPageMatchRegex.cap(1);
            QString matchOrEpisode = tournamentPageMatchRegex.cap(2);
            QString matchNumber = tournamentPageMatchRegex.cap(3);
            QString junk = tournamentPageMatchRegex.cap(4);
            QString matchDate = tournamentPageMatchRegex.cap(5).trimmed();

            junk.replace(tags, QString());
            junk.replace(QStringLiteral("&nbsp;"), QString());
            junk.replace(QStringLiteral("reveal match"), QString(), Qt::CaseInsensitive);
            junk.replace(QStringLiteral("reveal episode"), QString(), Qt::CaseInsensitive);
            junk.replace(QStringLiteral("host:"), QString(), Qt::CaseInsensitive);
            QStringList sides = junk.split(QStringLiteral("vs"));
            QString side1 = sides[0].trimmed();
            QString side2 = sides.size() > 1 ? sides[1].trimmed() : QString();
            if (side1.isEmpty()) {
                side1 = QStringLiteral("unknown");
            }

            QStringList parts = matchDate.split(QChar('/'), QString::SkipEmptyParts);
            if (parts.size() != 2) {
                qDebug() << "date format mismatch, expected month/day got " << matchDate;
                continue;
            }

            QDate qmatchDate(event.year(), parts[0].toInt(), parts[1].toInt());
            if (!qmatchDate.isValid()) {
                qDebug() << "date invalid, got" << matchDate;
                continue;
            }

            if (eventData.year == 0) {
                qDebug() << "setting event year from first match to" << qmatchDate.year();
                eventData.year = qmatchDate.year();
            }


            int imatchNumber = matchNumber.toInt();

            ScMatch match;
            ScMatchData& matchData = match.data();
            matchData.matchDate = qmatchDate;
            matchData.matchNumber = imatchNumber;
            matchData.side1 = side1;
            matchData.side2 = side2;
            matchData.name = matchOrEpisode + QStringLiteral(" ") + matchNumber;
            matchData.owner = stage.toWeak();

            completeRecord(eventData, stageData, matchData, &record);

            bool exclude = false;
            emit excludeRecord(record, &exclude);
            if (exclude) {
                qDebug() << "exclude" << record;
            } else {
//                t->isShow = matchOrEpisode.compare(QStringLiteral("match"), Qt::CaseInsensitive) == 0;
                stageData.matches << match;

                QNetworkReply* reply = makeRequest(link);
                m_RequestVod.insert(reply, match);
                m_PendingRequests.insert(reply, 2);
                ++m_TotalUrlsToFetched;
                qDebug() << "fetch match" << event.name() << event.year() << event.game() << event.season() << imatchNumber << qmatchDate << side1 << side2;
            }
        }

        if (!stageData.matches.isEmpty()) {
            eventData.stages << stage;
        }
    }
}

void
Sc2LinksDotCom::parseLevel2(QNetworkReply* reply) {
    QString soup = QString::fromUtf8(reply->readAll());
    //qDebug() << soup;

    ScMatch match = m_RequestVod.value(reply);
    int index = iFrameRegex.indexIn(soup);
    if (index >= 0) {
        QString url = iFrameRegex.cap(1);
        match.data().url = url;
        const ScStageData& stage = *match.data().owner.data();
        const ScEventData& event = *stage.owner.data();

        ScRecord record;
        completeRecord(event, stage, match.data(), &record);

        bool exclude = false;
        emit excludeRecord(record, &exclude);
        if (exclude) {
            qDebug() << "exclude" << record;
        } else {
            m_Vods << record;
        }

    } else {
        qWarning() << "no vod url found in" << reply->request().url();
    }
}
void
Sc2LinksDotCom::updateVodFetchingProgress() {
    if (m_TotalUrlsToFetched) {
        qreal progress = m_CurrentUrlsToFetched;
        progress /= m_TotalUrlsToFetched;
        setProgress(progress);
    } else {
        setProgress(0);
    }
}

void
Sc2LinksDotCom::_cancel() {
    qDebug() << "clearing event request queue";
    m_EventRequestQueue.clear();
    qDebug() << "canceling requests";
    foreach (const auto& key, m_PendingRequests.keys()) {
        key->abort();
    }
    // This crashes
//    const auto beg = m_PendingRequests.cbegin();
//    const auto end = m_PendingRequests.cend();
//    for (auto it = beg; it != end; ++it) {
//        it.key()->abort();
//    }
}

void
Sc2LinksDotCom::pruneInvalidSeries() {
    for (int h = 0; h < m_Events.size(); ++h) {
        ScEvent& event = m_Events[h];
        ScEventData& eventData = event.data();

        for (int i = 0; i < eventData.stages.size(); ++i) {
            ScStage& stage = eventData.stages[i];
            ScStageData& stageData = stage.data();
            for (int j = 0; j < stageData.matches.size(); ++j) {
                ScMatch& match = stageData.matches[j];
                if (match.url().isEmpty()) {
                    stageData.matches.removeAt(j--);
                }
            }

            if (stageData.matches.isEmpty()) {
                eventData.stages.removeAt(i--);
            }
        }

        if (eventData.stages.isEmpty()) {
            m_Events.removeAt(h--);
        }
    }
}

bool
Sc2LinksDotCom::canSkip() const {
    return true;
}

QString
Sc2LinksDotCom::_id() const {
    return QStringLiteral("sc2links.com");
}

void
Sc2LinksDotCom::completeRecord(const ScEventData& event, const ScStageData& stage, const ScMatchData& match, ScRecord* _record) const {
    Q_ASSERT(_record);
    toRecord(event, stage, match, _record);

    ScRecord& record = *_record;
    record.autoComplete(*classifier());

    if (!record.isValid(ScRecord::ValidGame)) {
        // focus on detecting bw
        record.game = ScRecord::GameSc2;
        record.valid |= ScRecord::ValidGame;
    }

    if (!record.isValid(ScRecord::ValidYear)) {
        record.year = record.matchDate.year();
        record.valid |= ScRecord::ValidYear;
    }
}


void
Sc2LinksDotCom::toRecord(const ScEventData& event, const ScStageData& stage, const ScMatchData& match, ScRecord* _record) {
    Q_ASSERT(_record);

    ScRecord& record = *_record;
    record.valid = 0;

    if (!match.url.isEmpty()) {
        record.url = match.url;
        record.valid |= ScRecord::ValidUrl;
    }

    if (!event.fullName.isEmpty()) {
        record.eventFullName = event.fullName;
        record.valid |= ScRecord::ValidEventFullName;
    }

    if (!event.name.isEmpty()) {
        record.eventName = event.name;
        record.valid |= ScRecord::ValidEventName;
    }

    if (event.year > 0) {
        record.year = event.year;
        record.valid |= ScRecord::ValidYear;
    }

    if (event.season > 0) {
        record.season = event.season;
        record.valid |= ScRecord::ValidSeason;
    }

    switch (event.game) {
    case ScEnums::Game_Broodwar:
        record.game = ScRecord::GameBroodWar;
        record.valid |= ScRecord::ValidGame;
        break;
    case ScEnums::Game_Sc2:
        record.game = ScRecord::GameSc2;
        record.valid |= ScRecord::ValidGame;
        break;
    }

    if (!stage.name.isEmpty()) {
        record.stage = stage.name;
        record.valid |= ScRecord::ValidStage;
    }

    if (!match.name.isEmpty()) {
        record.matchName = match.name;
        record.valid |= ScRecord::ValidMatchName;
    }

    if (match.matchDate.isValid()) {
        record.matchDate = match.matchDate;
        record.valid |= ScRecord::ValidMatchDate;
    }

    if (!match.side1.isEmpty()) {
        record.side1Name = match.side1;
        record.side2Name = match.side2;
        record.valid |= ScRecord::ValidSides;
    }

    if (match.matchNumber > 0) {
        record.matchNumber = match.matchNumber;
        record.valid |= ScRecord::ValidMatchNumber;
    }
}

void
Sc2LinksDotCom::saveRecords() {
    for (int i = 0, iend = m_Events.size(); i < iend; ++i) {
        const ScEvent& e = m_Events[i];
        const ScEventData& event = e.data();
        for (int j = 0, jend = event.stages.size(); j < jend; ++j) {
            const ScStage& s = event.stages[j];
            const ScStageData& stage = s.data();
            for (int k = 0, kend = stage.matches.size(); k < kend; ++k) {
                const ScMatch& m = stage.matches[k];
                const ScMatchData& match = m.data();

                m_Vods.push_back(ScRecord());
                toRecord(event, stage, match, &m_Vods.back());
            }
        }
    }
}
