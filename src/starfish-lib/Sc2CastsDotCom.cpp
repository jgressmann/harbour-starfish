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

#include <QNetworkRequest>
#include <QNetworkReply>
#include <QRegExp>
#include <QUrl>
#include <QUrlQuery>
#include <QDateTime>

#include "Sc2CastsDotCom.h"
#include "ScRecord.h"
#include "Sc.h"


namespace
{

const QString baseUrl = QStringLiteral("https://sc2casts.com");
const QRegExp pageNumberRegex(QStringLiteral("<title>[^>]*Page (\\d+)[^>]*</title>"));
const QRegExp iFrameRegex(QStringLiteral("<iframe\\s+[^>]*src\\s*=\\s*['\"]([^'\"]+)['\"][^>]*>"));
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
getMonth(const QString& str)
{
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
getDate(const QString& str)
{
    if (dateConstituentsRegex.indexIn(str) >= 0) {
        int day = dateConstituentsRegex.cap(2).toInt();
        int month = getMonth(dateConstituentsRegex.cap(1));
        int year = dateConstituentsRegex.cap(3).toInt();
        return QDate(year, month, day);
    }
    return QDate();
}

ScRecord::Race
getRace(const QString& str)
{
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

Sc2CastsDotCom::~Sc2CastsDotCom()
{
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
Sc2CastsDotCom::_fetch()
{
    m_Vods.clear();
    QNetworkReply* reply = makeRequest(makePageUrl(1), baseUrl);
    m_PendingRequests.insert(reply, 0);
    m_UrlsToFetch = 1;
    m_UrlsFetched = 0;
    m_CurrentPage = 1;
    //% "Fetching page %1"
    setProgressDescription(qtTrId("Sc2CastsDotCom-fetching-page").arg(1));
    updateVodFetchingProgress();
    qDebug("fetch started");
}


void
Sc2CastsDotCom::requestFinished(QNetworkReply* reply)
{
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

                auto newReply = makeRequest(newUrl, reply->url().toString());
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
                QNetworkReply* newReply = makeRequest(makePageUrl(m_CurrentPage), reply->url().toString());
                m_PendingRequests.insert(newReply, 0);
                //% "Fetching page %1"
                setProgressDescription(qtTrId("Sc2CastsDotCom-fetching-page").arg(m_CurrentPage));
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
Sc2CastsDotCom::parseLevel0(QNetworkReply* reply)
{
    if (m_CurrentPage == -1) {
        return;
    }

    auto response = Sc::decodeContent(reply);
    if (response.isEmpty()) {
        m_CurrentPage = -1;
        return;
    }

    QString soup = QString::fromUtf8(response);


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




    auto continueScraping = false;

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

    }

    dateGroupIndices << soup.size();

    ScRecord dateRecord;
    dateRecord.valid = ScRecord::ValidYear | ScRecord::ValidMatchDate;

    for (auto i = 1; i < dateGroupIndices.size(); ++i) {
        auto date = dates[i-1];

        if (!date.isValid()) {
            continue;
        }

        continueScraping = continueScraping || (yearToFetch() <= 0 || yearToFetch() <= date.year()); // sorted by date

        if (yearToFetch() <= 0 || yearToFetch() == date.year()) {

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

                QNetworkReply* level1Reply = makeRequest(baseUrl + relativeUrl, reply->url().toString());
                m_PendingRequests.insert(level1Reply, 1);
                m_ReplyToRecordTable.insert(level1Reply, m_Vods.size()-1);
                ++m_UrlsToFetch;
                updateVodFetchingProgress();
            }
        }
    }



    if (continueScraping) {
        ++m_CurrentPage;
    } else {
        if (yearToFetch() <= 0) {
            qDebug("%s\n", qPrintable(soup));
        }
        m_CurrentPage = -1;
    }
}

void
Sc2CastsDotCom::parseLevel1(QNetworkReply* reply)
{
    const auto index = m_ReplyToRecordTable.value(reply, -1);
    m_ReplyToRecordTable.remove(reply);
    ScRecord* record = &m_Vods[index];

    auto response = Sc::decodeContent(reply);
    if (response.isEmpty()) {
        record->valid = 0;
        return;
    }

    QString soup = QString::fromUtf8(response);



    // <div class="vslabel"><img src="//sc2casts.com/images/races/z_50.png" width="50" height="50" alt="Zerg" title="Zerg" style="padding-left:3px;padding-right:3px;"><h1><a href="/player452-Leenock"><b>Leenock</b></a> vs <a href="/player3246-Elazer"><b>Elazer</b></a></h1><img src="//sc2casts.com/images/races/z_50.png" width="50" height="50" alt="Zerg" title="Zerg" style="padding-left:3px;padding-right:3px;"></div><div class="videomenu"><div class="fn_buttons"><a href="javascript:void(0)" title="Hide YouTube controls to avoid duration spoilers" class="toggle_button_off" onclick="showSignUp()"><img src="/images/controls/duration_off.png" width="16" height="16" style="vertical-align:middle" >Hide duration</a></div></div><span class="games_links2"><div class="video_player_window"><iframe id="yytplayer" type="text/html" width="860" height="453" src="https://www.youtube.com/embed/SbIKVMf-qIM?start=0&showinfo=0" frameborder="0" allowfullscreen class="ytmargin"></iframe></div></span></div><div class="infolabel"><h2>BO3 <span class="nomobile">in 1 video</span></h2> from <h2><a href="/event986-2018-GSL-Season-2-Code-S"><span class="event_name">2018 GSL Season 2 Code S</span></a></h2>&nbsp;<h2><span class="round_name">Group Stage</span></h2><span class="nomobile">&nbsp;- </span><span class="onlymobile"></span>Caster: <h2><a href="/caster60-Artosis-&-tasteless"><span class="caster_name">Artosis & tasteless</span></a></h2><span class="nomobile">&nbsp;- </span><span class="onlymobile"></span>Posted on:&nbsp;<span>Apr 19, 2018</span></div><br/><div id="tt" style="padding-left: 10px;"><table style="font-size: 12px;"><tr><td align="center"><a href="javascript:void(0);" onclick="javascript:setrating('1',23339)"><img src="//sc2casts.com/images/thumbs_up.png" border=0 title="Yea" align="absmiddle"/></a><br/>0</td><td>&nbsp;</td><td align="center"><a href="javascript:void(0);" onclick="javascript:setrating('-1',23339)"><img src="//sc2casts.com/images/thumbs_down.png" border=0 title="Nah" align="absmiddle"/></a><br/>0</td><td valign="middle" style="font-size: 12px;margin-left: 10px;"><span style="display:inline-block; margin-left: 10px;">Enjoyed this series? Please vote and help others discover great <!-- google_ad_section_start -->Starcraft<!-- google_ad_section_end --> casts!</span></td></tr></table></div>

    //<span class="event_name">2018 GSL Season 2 Code S</span></a></h2>&nbsp;<h2><span class="round_name">Group Stage</span>

    // <div class="vslabel"><img src="//sc2casts.com/images/races/z_50.png" width="50" height="50" alt="Zerg" title="Zerg" style="padding-left:3px;padding-right:3px;"><h1><a href="/player3552-YoGo"><b>YoGo</b></a> vs <a href="/player3547-Funka"><b>Funka</b></a></h1><img src="//sc2casts.com/images/races/t_50.png" width="50" height="50" alt="Terran" title="Terran"></div><div class="videomenu"><div class="fn_buttons"><a href="javascript:void(0)" title="Turn On Continuous Play" class="toggle_button_off" onclick="showSignUp()"><img src="/images/controls/playlist_off.png" width="16" height="16" style="vertical-align:middle">Play all</a>&nbsp&nbsp;&nbsp;<a href="javascript:void(0)" title="Hide YouTube controls to avoid duration spoilers" class="toggle_button_off" onclick="showSignUp()"><img src="/images/controls/duration_off.png" width="16" height="16" style="vertical-align:middle" >Hide duration</a></div><span class="games_links2"><a href="javascript: void(0)" id="link1" onclick="javascript:toggleLayer2('g1', this)" class="selected">Game 1</a><a href="javascript: void(0)" id="link2" onclick="javascript:toggleLayer2('g2', this)">Game 2</a><a href="javascript: void(0)" id="link3" onclick="javascript:toggleLayer2('g3', this)">Game 3</a></span></div><div id="g1" class="video_player_window"><span class="games_links2"><div class="video_player_window"><iframe id="yytplayer" type="text/html" width="860" height="453" src="https://www.youtube.com/embed/3kGz1SK1mHc?start=0&showinfo=0" frameborder="0" allowfullscreen class="ytmargin"></iframe></div></span></div><div id="g2" style="display:none;" class="video_player_window"><span class="games_links2"><div class="video_player_window"><iframe id="yytplayer" type="text/html" width="860" height="453" src="https://www.youtube.com/embed/JxbwfV9qyW0?start=0&showinfo=0" frameborder="0" allowfullscreen class="ytmargin"></iframe></div></span></div><div id="g3" style="display:none;" class="video_player_window"><span class="games_links2"><div class="video_player_window"><iframe id="yytplayer" type="text/html" width="860" height="453" src="https://www.youtube.com/embed/aO9U4icbDWI?start=0&showinfo=0" frameborder="0" allowfullscreen class="ytmargin"></iframe></div></span></div></div><div class="infolabel"><h2>Best of 3</h2> from <h2><a href="/event1019-QLASH-Casters-Invitational"><span class="event_name">QLASH Casters Invitational</span></a></h2>&nbsp;<h2><span class="round_name">Group Stage</span></h2><span class="nomobile">&nbsp;- </span><span class="onlymobile"></span>Caster: <h2><a href="/caster751-ToD-&-Harstem"><span class="caster_name">ToD & Harstem</span></a></h2><span class="nomobile">&nbsp;- </span><span class="onlymobile"></span>Posted on:&nbsp;<span>Jan 05, 2019</span></div><br/><div id="tt" style="padding-left: 10px;"><table style="font-size: 12px;"><tr><td align="center"><a href="javascript:void(0);" onclick="javascript:setrating('1',24325)"><img src="//sc2casts.com/images/thumbs_up.png" border=0 title="Yea" align="absmiddle"/></a><br/>8</td><td>&nbsp;</td><td align="center"><a href="javascript:void(0);" onclick="javascript:setrating('-1',24325)"><img src="//sc2casts.com/images/thumbs_down.png" border=0 title="Nah" align="absmiddle"/></a><br/>1</td><td valign="middle" style="font-size: 12px;margin-left: 10px;"><span style="display:inline-block; margin-left: 10px;">Enjoyed this series? Please vote and help others discover great <!-- google_ad_section_start -->Starcraft<!-- google_ad_section_end --> casts!</span></td></tr></table></div>


    if (-1 == vslabelContent.indexIn(soup)) {
        record->valid = 0;
        qDebug("%s\n", qPrintable(soup));
        return;
    }

    auto content = vslabelContent.cap(1);
//        qDebug("%s\n", qPrintable(content));

    QString race1, race2;
    QString side1, side2;
    QString event;
    QString stage;
    QStringList urls;
    QUrl u;

    auto iFrameMatchIndex = iFrameRegex.indexIn(content);
    while (iFrameMatchIndex != -1) {
        auto url = iFrameRegex.cap(1);
        content.remove(iFrameRegex.cap(0));
        iFrameMatchIndex = iFrameRegex.indexIn(content);

        u.setUrl(url);
        if (u.isValid()) {
            if (u.isRelative()) {
                if (u.host() == QStringLiteral("sc2casts.com") &&
                    u.path() == QStringLiteral("/twitch/embed2")) {
                    QUrlQuery q(u);
                    int id = 0;
                    QString start;
                    if (q.hasQueryItem(QStringLiteral("id"))) {
                        id = q.queryItemValue(QStringLiteral("id")).toInt();
                    }
                    if (q.hasQueryItem(QStringLiteral("t"))) {
                        start = q.queryItemValue(QStringLiteral("t"));
                    }

                    if (id) {
                        url = QStringLiteral("https://player.twitch.tv/?video=v%1").arg(id);

                        if (!start.isEmpty()) {
                            url += QStringLiteral("&time=") + start;
                        }

                        urls << url;
                    }
                }
            } else {
                urls << url;
            }
        }
    }

    if (urls.isEmpty()) {
        record->valid = 0;
        qDebug("%s\n", qPrintable(reply->url().toString()));
        qDebug("%s\n", qPrintable(content));
        return;
    }


    if (eventNameRegex.indexIn(content) >= 0) {
        event = eventNameRegex.cap(1);
        content.remove(eventNameRegex.cap(0));
    }

    if (stageNameRegex.indexIn(content) >= 0) {
        stage = stageNameRegex.cap(1);
        content.remove(stageNameRegex.cap(0));
    }

    auto raceMatchIndex = raceRegex.indexIn(content);
    if (raceMatchIndex >= 0) {
        race1 = raceRegex.cap(1);
        content.remove(raceRegex.cap(0));
        raceMatchIndex = raceRegex.indexIn(content, raceMatchIndex);
        if (raceMatchIndex >= 0) {
            race2 = raceRegex.cap(1);
            content.remove(raceRegex.cap(0));
        }
    }

    auto playersMatchIndex = playersRegex.indexIn(content);
    if (playersMatchIndex >= 0) {
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
        record->valid |= ScRecord::ValidEventFullName;
        record->eventFullName = event;
    }

    if (!stage.isEmpty()) {
        record->valid |= ScRecord::ValidStageName;
        record->stage = stage;
    }

    if (!side1.isEmpty()) {
        record->valid |= ScRecord::ValidSides;
        record->side1Name = side1;
        record->side2Name = side2;
    }

    if (!race1.isEmpty()) {
        record->valid |= ScRecord::ValidRaces;
        record->side1Race = getRace(race1);
        record->side2Race = getRace(race2);
    }




    // first bw game in 2012
    if (record->isValid(ScRecord::ValidYear) &&
            !record->isValid(ScRecord::ValidGame) &&
            record->year < 2012) {
        record->valid |= ScRecord::ValidGame;
        record->game = ScRecord::GameSc2;
    }

    // attempt to get the year from the e.g. the event:
    // 2014 Proleague Preseason -> matches in 2013
    auto wasYearValid = record->isValid(ScRecord::ValidYear);
    auto year = record->year;
    record->valid &= ~ScRecord::ValidYear;


    record->autoComplete(*classifier());

    if ((wasYearValid && !record->isValid(ScRecord::ValidYear))) {
        record->year = year;
        record->valid |= ScRecord::ValidYear;
    }

    // focus on detecting brood war correctly
    if (!record->isValid(ScRecord::ValidGame)) {
        record->game = ScRecord::GameSc2;
        record->valid |= ScRecord::ValidGame;
    }

    for (auto i = 0; i < urls.size(); ++i) {

        if (!record) {
            m_Vods.push_back(m_Vods[index]);
            record = &m_Vods.back();
        }

        if (urls.size() > 1) {
            record->valid |= ScRecord::ValidMatchNumber;
            record->matchNumber = static_cast<qint8>(i + 1);
        }

        record->valid |= ScRecord::ValidUrl;
        record->url = urls[i];

        if (record->isValid(
                    ScRecord::ValidEventName |
                    ScRecord::ValidEventFullName |
                    ScRecord::ValidStageName |
                    ScRecord::ValidYear |
                    ScRecord::ValidUrl |
                    ScRecord::ValidMatchDate |
                    ScRecord::ValidMatchName)) {
            bool exists = false;
            emit hasRecord(*record, &exists);
            if (exists) {
                record->valid = 0;
                // done, sc2casts are newest first, always
                m_CurrentPage = -1;
            } else {
                bool exclude = false;
                emit excludeRecord(*record, &exclude);
                if (exclude) {
    //                    qDebug() << "exclude" << record;
                    record->valid = 0;
                } else {
                    record = nullptr;
                }
            }
        } else {
            //qDebug() << "invalid" << record;
            record->valid = 0;
            //qDebug("%s\n", qPrintable(content));
        }
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

    // aparently this causes replies to go to finished within this call, even
    // if called from dtor
    foreach (const auto& key, m_PendingRequests.keys()) {
        key->abort();
    }

    m_PendingRequests.clear();
    m_ReplyToRecordTable.clear();

}

QString
Sc2CastsDotCom::makePageUrl(int page) const
{
    return baseUrl + QString::asprintf("/all:page=%d", page);
}

void
Sc2CastsDotCom::pruneInvalidVods() {
    // remove invalid vods
    for (int i = 0; i < m_Vods.size(); ) {
        // check for url instead of != 0 to remove those
        // entries that were added during level 0 parsing but never
        // got to level 1.
        if (m_Vods[i].isValid(ScRecord::ValidUrl)) {
            ++i;
        } else {
            m_Vods[i] = m_Vods.back();
            m_Vods.pop_back();
        }
    }
}

QString
Sc2CastsDotCom::_id() const {
    return QStringLiteral("sc2casts.com");
}
