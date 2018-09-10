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

#include "Sc2CastsDotCom.h"
#include "ScRecord.h"
#include "Sc.h"


namespace  {

const QString baseUrl = QStringLiteral("https://sc2casts.com");
const QRegExp pageNumberRegex(QStringLiteral("<title>[^>]*Page (\\d+)[^>]*</title>"));
const QRegExp iFrameRegex(QStringLiteral("<iframe\\s+(?:[^>]+\\s+)*src\\s*=\\s*['\"]([^'\"]+)['\"][^>]*>"));
const QRegExp tags(QStringLiteral("<[^>]+>"));
const QRegExp dateConstituentsRegex("(\\w+)\\s+(\\d{1,2})\\s*,?\\s*(\\d{4})");
const QRegExp eventNameRegex("<span\\s+class\\s*=\\s*['\"]event_name['\"][^>]*>([^>]*)</span>");
const QRegExp stageNameRegex("<span\\s+class\\s*=\\s*['\"]round_name['\"][^>]*>([^>]*)</span>");
const QRegExp vslabelContent("<div\\s+(?:\\s*[^>]*)*class\\s*=\\s*['\"]vslabel['\"][^>]*>(.*Posted on.*)</div>");
const QRegExp raceRegex("<img\\s+(?:\\s*[^>]*)*alt\\s*=\\s*['\"]([^'\"]+)['\"][^>]*>");
const QRegExp playersRegex("<h1>(.*)</h1>");
const QRegExp dateGroupRegex("<div\\s+(?:\\s*[^>]*)*class\\s*=\\s*['\"]headline_mobile['\"][^>]*>([^>]+)</div>");
const QRegExp matchUrlRegex("<h2>.*<a\\s+(?:\\s*[^>]*)*href\\s*=\\s*['\"](/cast\\d+[^'\"]+)['\"](?:\\s*[^>]*)*>(.*)</h2>");
const QRegExp matchNameRegex("</span><b\\s*>(.*)</b><span");



int InitializeStatics() {
    Q_ASSERT(pageNumberRegex.isValid());
    Q_ASSERT(tags.isValid());
    Q_ASSERT(iFrameRegex.isValid());
    Q_ASSERT(dateConstituentsRegex.isValid());
    Q_ASSERT(eventNameRegex.isValid());
    Q_ASSERT(stageNameRegex.isValid());
    Q_ASSERT(vslabelContent.isValid());
    Q_ASSERT(raceRegex.isValid());
    Q_ASSERT(playersRegex.isValid());
    Q_ASSERT(dateGroupRegex.isValid());
    Q_ASSERT(matchUrlRegex.isValid());
    Q_ASSERT(matchNameRegex.isValid());
    const_cast<QRegExp&>(vslabelContent).setMinimal(true);
    const_cast<QRegExp&>(playersRegex).setMinimal(true);
    const_cast<QRegExp&>(matchNameRegex).setMinimal(true);
    const_cast<QRegExp&>(matchUrlRegex).setMinimal(true);
    return 1;
}

int s_StaticsInitialized = InitializeStatics();

int
getMonth(const QString& str) {
    if (str.startsWith(QStringLiteral("jan"), Qt::CaseInsensitive)) {
        return 1;
    }

    if (str.startsWith(QStringLiteral("feb"), Qt::CaseInsensitive)) {
        return 2;
    }

    if (str.startsWith(QStringLiteral("mar"), Qt::CaseInsensitive)) {
        return 3;
    }

    if (str.startsWith(QStringLiteral("apr"), Qt::CaseInsensitive)) {
        return 4;
    }

    if (str.startsWith(QStringLiteral("may"), Qt::CaseInsensitive)) {
        return 5;
    }

    if (str.startsWith(QStringLiteral("jun"), Qt::CaseInsensitive)) {
        return 6;
    }

    if (str.startsWith(QStringLiteral("jul"), Qt::CaseInsensitive)) {
        return 7;
    }

    if (str.startsWith(QStringLiteral("aug"), Qt::CaseInsensitive)) {
        return 8;
    }

    if (str.startsWith(QStringLiteral("sep"), Qt::CaseInsensitive)) {
        return 9;
    }

    if (str.startsWith(QStringLiteral("oct"), Qt::CaseInsensitive)) {
        return 10;
    }

    if (str.startsWith(QStringLiteral("nov"), Qt::CaseInsensitive)) {
        return 11;
    }

    if (str.startsWith(QStringLiteral("dec"), Qt::CaseInsensitive)) {
        return 12;
    }

    return 0;
}


QDate
getDate(const QString& str) {
    if (dateConstituentsRegex.indexIn(str) >= 0) {
        int day = dateConstituentsRegex.cap(2).toInt();
        int month = getMonth(dateConstituentsRegex.cap(1));
        int year = dateConstituentsRegex.cap(3).toInt();
        return QDate(year, month, day);
    }
    return QDate();
}

ScRecord::Race
getRace(const QString& str) {
    if (str.startsWith(QStringLiteral("protoss"), Qt::CaseInsensitive)) {
        return ScRecord::RaceProtoss;
    }

    if (str.startsWith(QStringLiteral("terran"), Qt::CaseInsensitive)) {
        return ScRecord::RaceTerran;
    }

    if (str.startsWith(QStringLiteral("zerg"), Qt::CaseInsensitive)) {
        return ScRecord::RaceZerg;
    }

    return ScRecord::RaceUnknown;
}

} // anon

Sc2CastsDotCom::~Sc2CastsDotCom() {
    abort();
}

Sc2CastsDotCom::Sc2CastsDotCom(QObject *parent)
    : ScVodScraper(parent)
{
    m_UrlsToFetch = 0;
    m_UrlsFetched = 0;
    m_CurrentPage = -1;
    (void)s_StaticsInitialized;

    connect(networkAccessManager(), SIGNAL(finished(QNetworkReply*)), this, SLOT(requestFinished(QNetworkReply*)));
}

void
Sc2CastsDotCom::_fetch() {
    m_Vods.clear();
    QNetworkReply* reply = makeRequest(makePageUrl(1));
    m_PendingRequests.insert(reply, 0);
    m_UrlsToFetch = 1;
    m_UrlsFetched = 0;
    m_CurrentPage = 0;

    setProgressDescription(tr("Fetching page 1"));
    updateVodFetchingProgress();
    qDebug("fetch started");
}

QNetworkReply*
Sc2CastsDotCom::makeRequest(const QUrl& url) const {
    return networkAccessManager()->get(Sc::makeRequest(url));
}

void
Sc2CastsDotCom::requestFinished(QNetworkReply* reply) {
    QMutexLocker g(lock());
    reply->deleteLater();
    const int level = m_PendingRequests.value(reply, -1);
    m_PendingRequests.remove(reply);

    switch (status()) {
    case Status_VodFetchingInProgress:
    case Status_VodFetchingBeingCanceled:
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

                auto newReply = makeRequest(newUrl);
                m_PendingRequests.insert(newReply, level);
                auto it = m_ReplyToRecordTable.find(reply);
                if (it != m_ReplyToRecordTable.end()) {
                    m_ReplyToRecordTable.insert(newReply, it.value());
                    m_ReplyToRecordTable.erase(it);
                }
                ++m_UrlsToFetch;
            } else  {
                qDebug() << "Http status code:" << v;
            }
        } break;
        default:
            qDebug() << "Network request failed: " << reply->errorString();
            break;
        }

        ++m_UrlsFetched;
        updateVodFetchingProgress();


        if (m_PendingRequests.isEmpty()) {
            if (m_CurrentPage >= 1) {
                ++m_UrlsToFetch;
                QNetworkReply* reply = makeRequest(makePageUrl(m_CurrentPage));
                m_PendingRequests.insert(reply, 0);
                setProgressDescription(QString::asprintf("Fetching page %d", m_CurrentPage));
                updateVodFetchingProgress();
            } else {
                m_ReplyToRecordTable.clear();

                pruneInvalidVods();

                setStatus(status() == Status_VodFetchingInProgress ? Status_VodFetchingComplete : Status_VodFetchingCanceled);

                setProgressDescription(QString());
                qDebug("fetch finished");
            }
        }
        break;
    default:
        if (m_PendingRequests.isEmpty()) {
            m_ReplyToRecordTable.clear();
            m_Vods.clear();
            setStatus(Status_Ready);
        }
        break;
    }
}

void
Sc2CastsDotCom::parseLevel0(QNetworkReply* reply) {
    QString soup = QString::fromUtf8(reply->readAll());

    if (m_CurrentPage == -1) {
        return;
    }

    auto index = pageNumberRegex.indexIn(soup);
    if (index == -1) {
        qDebug("%s\n", qPrintable(soup));
        return;
    }

    auto pageNumber = pageNumberRegex.cap(1).toInt();
    if (pageNumber <= 0 || pageNumber < m_CurrentPage) {
        // done
        m_CurrentPage = -1;
        qDebug("%s\n", qPrintable(soup));
        return;
    }

    // <div style="padding-top: 10px;" class="headline_mobile">Wed Mar 28, 2018</div>

    QList<int> dateGroupIndices;
    QList<QDate> dates;




    auto any = false;

    for (int start = 0, found = dateGroupRegex.indexIn(soup, start);
         found != -1;
         start = found + dateGroupRegex.cap(0).length(),
         found = dateGroupRegex.indexIn(soup, start)) {

        dateGroupIndices << found;
        auto dateSoup = dateGroupRegex.cap(1);
        if (dateSoup.contains(QStringLiteral("today"), Qt::CaseInsensitive)) {
            dateSoup = QDate::currentDate().toString(QStringLiteral("MMM dd, yyyy"));
        } else if (dateSoup.contains(QStringLiteral("yesterday"), Qt::CaseInsensitive)) {
            dateSoup = QDate::currentDate().addDays(-1).toString(QStringLiteral("MMM dd, yyyy"));
        }
        auto date = getDate(dateSoup);
        dates << date;

        any = true;
    }

    dateGroupIndices << soup.size();

    ScRecord dateRecord;
    dateRecord.valid = ScRecord::ValidYear | ScRecord::ValidMatchDate;

    for (auto i = 1; i < dateGroupIndices.size(); ++i) {
        auto date = dates[i-1];

        if (!date.isValid()) {
            continue;
        }

        dateRecord.year = date.year();
        dateRecord.matchDate = date;

        bool exclude = false;
        emit excludeRecord(dateRecord, &exclude);
        if (exclude) {
            continue;
        }

        const auto matchSoup = soup.mid(dateGroupIndices[i-1], dateGroupIndices[i] - dateGroupIndices[i-1]);
        for (int start = 0, found = matchUrlRegex.indexIn(matchSoup, start);
             found != -1;
             start = found + matchUrlRegex.cap(0).length(),
             found = matchUrlRegex.indexIn(matchSoup, start)) {


            auto relativeUrl = matchUrlRegex.cap(1);
            auto junk = matchUrlRegex.cap(2);
//            qDebug("%s\n", qPrintable(junk));

            if (matchNameRegex.indexIn(junk) == -1) {
                continue;
            }

            ScRecord record = dateRecord;
            record.matchName = matchNameRegex.cap(1).remove(tags).split(' ', QString::SkipEmptyParts).join(QStringLiteral(" "));
            record.valid |= ScRecord::ValidMatchName;

//            qDebug() << record.matchName << record.matchDate;

            m_Vods << record;

            QNetworkReply* level1Reply = makeRequest(baseUrl + relativeUrl);
            m_PendingRequests.insert(level1Reply, 1);
            m_ReplyToRecordTable.insert(level1Reply, m_Vods.size()-1);
            ++m_UrlsToFetch;
            updateVodFetchingProgress();
        }
    }



    if (any) {
        ++m_CurrentPage;

    } else {
        qDebug("%s\n", qPrintable(soup));
        m_CurrentPage = -1;
    }


//    // Multiple Players vs Multiple Players (Games in 1 Video) - 2018 Starcraft 1 ASL S5 - Round of 16 / cast by: Artosis & tasteless
//    //<div style="padding-top: 10px;" class="headline_mobile">Yesterday</div>
//    // <h2><span class="play_series_mobile"><a href="/cast23335-Multiple-Players-vs-Multiple-Players-Games-in-1-Video-2018-Starcraft-1-ASL-S5-Round-of-16"><span style="color:#cccccc;" class="onlymobile inline">&nbsp;</span><b >Multiple Players</b> vs <b >Multiple Players</b><span class="nomobile"> (Games in 1 Video)</span></a></span></h2>
//    // <h3><a href="/event980-2018-Starcraft-1-ASL-S5"><span class="event_name"  style="color:#brown">2018 Starcraft 1 ASL S5</span></a></h3>&nbsp;-&nbsp;<h3>
//    // <h3><span class="round_name">Round of 16</span></h3>

//    const QRegExp dateGroupRegex("<div\\s+(?:\\s*[^>]*)*class\\s*=\\s*['\"]headline_mobile['\"](?:\\s*[^>]*)*>([^>]+)</div>");
//    Q_ASSERT(dateGroupRegex.isValid());
//    const QRegExp matchHeaderRegex("<div\\s+(?:\\s*[^>]*)*class=['\"]latest_series['\"](?:\\s*[^>]*)*>");
//    Q_ASSERT(matchHeaderRegex.isValid());
//    QRegExp playersRegex("<h2>.*<a\\s+href=['\"]([^'\"]+)['\"]\\s*>(.*)</h2>");
//    playersRegex.setMinimal(true);
//    Q_ASSERT(playersRegex.isValid());

//    const QRegExp eventNameRegex("<span\\s+class=['\"]event_name['\"](?:\\s*[^>]*)*>([^>]*)</span>");
//    Q_ASSERT(eventNameRegex.isValid());
//    const QRegExp stageNameRegex("<span\\s+class=['\"]round_name['\"](?:\\s*[^>]*)*>([^>]*)</span>");
//    Q_ASSERT(stageNameRegex.isValid());

//    const QRegExp videoFormatRegex("\\([^)]+\\s+\\d+\\s+video\\s*\\)", Qt::CaseInsensitive);
//    Q_ASSERT(videoFormatRegex.isValid());

//    const QRegExp racesRegex("\\[[^\\]]+\\]");
//    Q_ASSERT(racesRegex.isValid());



//    bool any = false;
//    ScRecord record;
//    int lastDateGroupIndex = -1;
//    QString lastDate;
//    for (int start = 0, found = dateGroupRegex.indexIn(soup, start);
//         found != -1;
//         lastDateGroupIndex = found,
//         start = found + dateGroupRegex.cap(0).length(),
//         found = dateGroupRegex.indexIn(soup, start)) {

//        any = true;

//        if (lastDateGroupIndex != -1) {
//            auto matchesSoup = soup.mid(lastDateGroupIndex, found - lastDateGroupIndex);

////            qDebug("%s\n", qPrintable(matchesSoup));
//            int lastMatchHeaderIndex = -1;
//            for (int start2 = 0, found2 = matchHeaderRegex.indexIn(matchesSoup, start2);
//                 found2 != -1;
//                 lastMatchHeaderIndex = found2,
//                 start2 = found2 + matchHeaderRegex.cap(0).length(),
//                 found2 = matchHeaderRegex.indexIn(matchesSoup, start2)) {

//                if (lastMatchHeaderIndex != -1) {
//                    auto matchSoup = matchesSoup.mid(lastMatchHeaderIndex, found2 - lastMatchHeaderIndex);

////                    qDebug("%s\n", qPrintable(matchSoup));
//                    QString event;
//                    QString stage;
//                    QString side1, side2;
//                    QString matchUrl;

//                    if (eventNameRegex.indexIn(matchesSoup) >= 0) {
//                        event = eventNameRegex.cap(1);
//                    }

//                    if (stageNameRegex.indexIn(matchesSoup) >= 0) {
//                        stage = stageNameRegex.cap(1);
//                    }

//                    if (playersRegex.indexIn(matchesSoup) >= 0) {
//                        matchUrl = playersRegex.cap(1);
//                        auto junk = playersRegex.cap(2);
//                        junk.remove(tags);
//                        junk.remove(videoFormatRegex);
//                        junk.remove(racesRegex);
//                        junk.replace(QStringLiteral("&nbsp;"), QString(" "));
//                        auto sides = junk.split(QStringLiteral(" vs "));
//                        side1 = sides[0].trimmed();
//                        if (sides.size() > 1) {
//                            side2 = sides[1].trimmed();
//                        }
//                    }

//                    qDebug() << event << stage << side1 << side2 << matchUrl;
//                }
//            }
//        }
//    }

//    if (any) {
//        qDebug("%s\n", qPrintable(soup));
//    } else {
//        ++m_CurrentPage;
//        QNetworkReply* reply = makeRequest(makePageUrl(m_CurrentPage));
//        m_PendingRequests.insert(reply, 0);
//        ++m_TotalUrlsToFetched;
//        ++m_CurrentUrlsToFetched;
//        m_CurrentPage = 0;
//        setProgressDescription(QString::asprintf("Fetching page %d", m_CurrentPage));
//        updateVodFetchingProgress();
//    }
}

void
Sc2CastsDotCom::parseLevel1(QNetworkReply* reply) {
    auto index = m_ReplyToRecordTable.value(reply, -1);
    m_ReplyToRecordTable.remove(reply);

//    qDebug() << index;
    ScRecord& record = m_Vods[index];
//    qDebug() << record;

    QString soup = QString::fromUtf8(reply->readAll());

    // <div class="vslabel"><img src="//sc2casts.com/images/races/z_50.png" width="50" height="50" alt="Zerg" title="Zerg" style="padding-left:3px;padding-right:3px;"><h1><a href="/player452-Leenock"><b>Leenock</b></a> vs <a href="/player3246-Elazer"><b>Elazer</b></a></h1><img src="//sc2casts.com/images/races/z_50.png" width="50" height="50" alt="Zerg" title="Zerg" style="padding-left:3px;padding-right:3px;"></div><div class="videomenu"><div class="fn_buttons"><a href="javascript:void(0)" title="Hide YouTube controls to avoid duration spoilers" class="toggle_button_off" onclick="showSignUp()"><img src="/images/controls/duration_off.png" width="16" height="16" style="vertical-align:middle" >Hide duration</a></div></div><span class="games_links2"><div class="video_player_window"><iframe id="yytplayer" type="text/html" width="860" height="453" src="https://www.youtube.com/embed/SbIKVMf-qIM?start=0&showinfo=0" frameborder="0" allowfullscreen class="ytmargin"></iframe></div></span></div><div class="infolabel"><h2>BO3 <span class="nomobile">in 1 video</span></h2> from <h2><a href="/event986-2018-GSL-Season-2-Code-S"><span class="event_name">2018 GSL Season 2 Code S</span></a></h2>&nbsp;<h2><span class="round_name">Group Stage</span></h2><span class="nomobile">&nbsp;- </span><span class="onlymobile"></span>Caster: <h2><a href="/caster60-Artosis-&-tasteless"><span class="caster_name">Artosis & tasteless</span></a></h2><span class="nomobile">&nbsp;- </span><span class="onlymobile"></span>Posted on:&nbsp;<span>Apr 19, 2018</span></div><br/><div id="tt" style="padding-left: 10px;"><table style="font-size: 12px;"><tr><td align="center"><a href="javascript:void(0);" onclick="javascript:setrating('1',23339)"><img src="//sc2casts.com/images/thumbs_up.png" border=0 title="Yea" align="absmiddle"/></a><br/>0</td><td>&nbsp;</td><td align="center"><a href="javascript:void(0);" onclick="javascript:setrating('-1',23339)"><img src="//sc2casts.com/images/thumbs_down.png" border=0 title="Nah" align="absmiddle"/></a><br/>0</td><td valign="middle" style="font-size: 12px;margin-left: 10px;"><span style="display:inline-block; margin-left: 10px;">Enjoyed this series? Please vote and help others discover great <!-- google_ad_section_start -->Starcraft<!-- google_ad_section_end --> casts!</span></td></tr></table></div>

    //<span class="event_name">2018 GSL Season 2 Code S</span></a></h2>&nbsp;<h2><span class="round_name">Group Stage</span>



    if (vslabelContent.indexIn(soup) >= 0) {
        auto content = vslabelContent.cap(1);
//        qDebug("%s\n", qPrintable(content));

        QString race1, race2;
        QString side1, side2;
        QString event;
        QString stage;
//        QDate matchDate;
        QString url;

//        auto index = matchDateRegex.indexIn(content);
//        if (index >= 0) {
//            auto junk = matchDateRegex.cap(1);
//            junk.remove(tags);
//            junk.replace(QStringLiteral("&nbsp;"), QStringLiteral(" "));

//            matchDate = getDate(junk);

//            content.resize(index);
//        }

        if (iFrameRegex.indexIn(content) >= 0) {
            url = iFrameRegex.cap(1);
            content.remove(iFrameRegex.cap(0));
        }


        if (eventNameRegex.indexIn(content) >= 0) {
            event = eventNameRegex.cap(1);
            content.remove(eventNameRegex.cap(0));
        }

        if (stageNameRegex.indexIn(content) >= 0) {
            stage = stageNameRegex.cap(1);
            content.remove(stageNameRegex.cap(0));
        }

        index = raceRegex.indexIn(content);
        if (index >= 0) {
            race1 = raceRegex.cap(1);
            content.remove(raceRegex.cap(0));
            index = raceRegex.indexIn(content, index);
            if (index >= 0) {
                race2 = raceRegex.cap(1);
                content.remove(raceRegex.cap(0));
            }
        }

        index = playersRegex.indexIn(content);
        if (index >= 0) {
            auto playersContent = playersRegex.cap(1);
            playersContent.remove(tags);
            playersContent.replace(QStringLiteral("&nbsp;"), QStringLiteral(" "));
            auto sides = playersContent.split(QStringLiteral(" vs "));
            side1 = sides[0].trimmed();
            if (sides.size() > 1) {
                side2 = sides[1].trimmed();
            }

//            content.remove(playersRegex.cap(0));
        }



//        content.remove(tags);


        if (!event.isEmpty()) {
            record.valid |= ScRecord::ValidEventFullName;
            record.eventFullName = event;
        }

        if (!stage.isEmpty()) {
            record.valid |= ScRecord::ValidStage;
            record.stage = stage;
        }

        if (!side1.isEmpty()) {
            record.valid |= ScRecord::ValidSides;
            record.side1Name = side1;
            record.side2Name = side2;
        }

        if (!race1.isEmpty()) {
            record.valid |= ScRecord::ValidRaces;
            record.side1Race = getRace(race1);
            record.side2Race = getRace(race2);
        }

//        if (matchDate.isValid()) {
//            record.valid |= ScRecord::ValidMatchDate;
//            record.matchDate = matchDate;
//        }

        if (!url.isEmpty()) {
            record.valid |= ScRecord::ValidUrl;
            record.url = url;
        }

        // first bw game in 2012
        if (record.isValid(ScRecord::ValidYear) &&
                !record.isValid(ScRecord::ValidGame) &&
                record.year < 2012) {
            record.valid |= ScRecord::ValidGame;
            record.game = ScRecord::GameSc2;
        }

        // attempt to get the year from the e.g. the event:
        // 2014 Proleague Preseason -> matches in 2013
        auto wasYearValid = record.isValid(ScRecord::ValidYear);
        auto year = record.year;
        record.valid &= ~ScRecord::ValidYear;


        record.autoComplete(*classifier());

        if ((wasYearValid && !record.isValid(ScRecord::ValidYear))) {
            record.year = year;
            record.valid |= ScRecord::ValidYear;
        }

        // focus on detecting brood war correctly
        if (!record.isValid(ScRecord::ValidGame)) {
            record.game = ScRecord::GameSc2;
            record.valid |= ScRecord::ValidGame;
        }

        if (record.isValid(
                    ScRecord::ValidEventName |
                    ScRecord::ValidEventFullName |
                    ScRecord::ValidStage |
                    ScRecord::ValidYear |
                    ScRecord::ValidUrl |
                    ScRecord::ValidMatchDate |
                    ScRecord::ValidMatchName)) {
            bool exists = false;
            emit hasRecord(record, &exists);
            if (exists) {
                record.valid = 0;
                // done
                m_CurrentPage = -1;
            } else {
                bool exclude = false;
                emit excludeRecord(record, &exclude);
                if (exclude) {
//                    qDebug() << "exclude" << record;
                    record.valid = 0;
                } else {

                }
            }
        } else {
            //qDebug() << "invalid" << record;
            record.valid = 0;
            //qDebug("%s\n", qPrintable(content));
        }
    } else {
        record.valid = 0;
        qDebug("%s\n", qPrintable(soup));
    }
}




void
Sc2CastsDotCom::updateVodFetchingProgress() {
    if (m_UrlsToFetch) {
        qreal progress = m_UrlsFetched;
        progress /= m_UrlsToFetch;
        setProgress(progress);
    } else {
        setProgress(0);
    }
}

void
Sc2CastsDotCom::_cancel() {
    qDebug() << "canceling level 0 fetches";
    m_CurrentPage = -1;
    if (m_PendingRequests.isEmpty()) {
        setStatus(Status_VodFetchingCanceled);
    }
}

QString
Sc2CastsDotCom::makePageUrl(int page) const {
    return baseUrl + QString::asprintf("/all:page=%d", page);
}

void
Sc2CastsDotCom::pruneInvalidVods() {
    // remove invalid vods
    for (int i = 0; i < m_Vods.size(); ++i) {
        if (!m_Vods[i].valid) {
            m_Vods[i] = m_Vods[m_Vods.size()-1];
            m_Vods.pop_back();
            --i;
        }
    }
}

QString
Sc2CastsDotCom::_id() const {
    return QStringLiteral("sc2casts.com");
}
