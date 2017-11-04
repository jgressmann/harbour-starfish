#include "Sc2LinksDotCom.h"
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QRegExp>
#include <QMutexLocker>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QSqlRecord>
#include <vector>
#include <string>
#include <utility>

#ifndef _countof
#   define _countof(x) (sizeof(x)/sizeof((x)[0]))
#endif

namespace  {
//https://stackoverflow.com/questions/12323297/converting-a-string-to-roman-numerals-in-c
typedef std::pair<int, std::string> valueMapping;
static std::vector<valueMapping> importantNumbers = {
    {1000, "M"}, {900, "CM"}, {500, "D"}, {400, "CD"},
    {100,  "C"}, { 90, "XC"}, { 50, "L"}, { 40, "XL"},
    {10,   "X"}, {  9, "IX"}, {  5, "V"}, {  4, "IV"},
    {1,    "I"},
};

QString romanNumeralFor(int n, int markCount = 0) {
    std::string result;
    bool needMark = false;
    std::string marks(markCount, '\'');
    for (auto mapping : importantNumbers) {
        int value = mapping.first;
        std::string &digits = mapping.second;
        while (n >= value) {
            result += digits;
            n -= value;
            needMark = true;
        }
        if ((value == 1000 || value == 100 || value == 10 || value == 1) && needMark) {
            result += marks;
            needMark = false;
        }
    }
    return QString::fromStdString(result);
}

const QString EnglishWordSeasons[] = {
    QStringLiteral("season one"),
    QStringLiteral("season two"),
    QStringLiteral("season three"),
    QStringLiteral("season four"),
    QStringLiteral("season five"),
    QStringLiteral("season six"),
    QStringLiteral("season seven"),
    QStringLiteral("season eight"),
    QStringLiteral("season nine"),
    QStringLiteral("season ten"),
};


const QString EntryUrl = QStringLiteral("https://www.sc2links.com/vods/");


//                                      <a href="      https://www.sc2links.com/tournament/?match=502"           >WCS Montreal</a></div><div    class="    voddate"         >September 11th 2017</div></br>
const QRegExp aHrefRegex(QStringLiteral("<a href=['\"](https://www.sc2links.com/tournament/\\?match=\\d+)['\"][^>]*>([^>]+)</a></div><div\\s+class=['\"]voddate['\"][^>]*>([^>]+)</div>"));
const QRegExp tournamentPageStageNameRegex(QStringLiteral("<h3>([^>]+)</h3>\\s*<h5>"));
const QRegExp tournamentPageStageContent(QStringLiteral("<h3>([^>]+)</h3>\\s*(<h5>.*)<h"));
// <a  href="https://www.sc2links.com/match/?match=40754"><div class="match">Match 1</div></a>
const QRegExp tournamentPageMatchRegex(QStringLiteral("<h5>\\s*<a\\s+href=['\"](https://www.sc2links.com/match/\\?match=\\d+)['\"][^>]*><div\\s+class\\s*=\\s*['\"]match['\"]\\s*>(\\w+)\\s+(\\d+)</div>\\s*</a>.*<div\\s+class=['\"]date['\"]\\s*>([^>]+)</div>\\s*</h5>"));
const QRegExp iFrameRegex(QStringLiteral("<iframe\\s+(?:[^>]+\\s+)*src=['\"]([^'\"]+)['\"][^>]*>"));

const QRegExp dateRegex(QStringLiteral("\\d{4}-\\d{2}-\\d{2}"));
const QRegExp fuzzyYearRegex(QStringLiteral(".*(\\d{4}).*"));
const QRegExp yearRegex(QStringLiteral("\\d{4}"));
const QRegExp tags(QStringLiteral("<[^>]+>"));


typedef QHash<QString, QString> TournamentToIconMapType;
const TournamentToIconMapType TournamentToIconMap;

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

    TournamentToIconMapType& m = const_cast< TournamentToIconMapType& >(TournamentToIconMap);
    m.insert("gsl", "gsl.png");
    m.insert("asl", "asl.png");
    m.insert("afreeca starleague", "asl.png");
    m.insert("hsc", "hsc.png");
    m.insert("home story cup", "hsc.png");
    m.insert("iron squid", "iron_squid.png");

    return 1;
}

int s_StaticsInitialized = InitializeStatics();

} // anon

const QByteArray VodModel::ms_UserAgent("MyAppName/1.0 (Nokia; Qt)");

VodModel::~VodModel() {
    setNetworkAccessManager(Q_NULLPTR);
}

VodModel::VodModel(QObject *parent)
    : QObject(parent)
    , m_Lock(QMutex::Recursive) {

    m_Database = Q_NULLPTR;
    m_Manager = Q_NULLPTR;
    m_Status = Status_Uninitialized;
    m_Error = Error_None;
    m_VodFetchingProgress = 0;
    m_TotalUrlsToFetched = 0;
    m_CurrentUrlsToFetched = 0;
    m_AddedVods = false;

    connect(&m_Timer, SIGNAL(timeout()), this, SLOT(poll()));
    m_Timer.setInterval(30000);
    m_Timer.setTimerType(Qt::CoarseTimer);
}

void
VodModel::poll() {
    switch (m_Status) {
    case Status_Error:
        qWarning() << "Not polling, error: " << qPrintable(errorString());
        return;
    case Status_Ready:
    case Status_VodFetchingComplete:
        break;
    case Status_Uninitialized:
        qWarning("Not initialized");
        return;
    case Status_VodFetchingInProgress:
        qWarning("Poll alreay in progress");
        return;
    default:
        qWarning() << "Error: " << qPrintable(errorString());
        return;
    }

    QMutexLocker guard(&m_Lock);



    QNetworkReply* reply = makeRequest(EntryUrl);
    m_PendingRequests.insert(reply, 0);
    setStatus(Status_VodFetchingInProgress);
    m_TotalUrlsToFetched = 1;
    m_CurrentUrlsToFetched = 0;
    m_AddedVods = false;
    updateVodFetchingProgress();
    qDebug("poll started");
}

QNetworkReply*
VodModel::makeRequest(const QUrl& url) const {
    QNetworkRequest request;
    request.setUrl(url);
    request.setRawHeader("User-Agent", ms_UserAgent);// must be set else no reply

    return m_Manager->get(request);
}

void
VodModel::setNetworkAccessManager(QNetworkAccessManager *manager) {
    QMutexLocker guard(&m_Lock);
    if (m_Manager != manager) {
        if (m_Manager) {
            m_Timer.stop();
//            deleteCurrentRequest();
            disconnect(m_Manager, SIGNAL(finished(QNetworkReply*)), this, SLOT(requestFinished(QNetworkReply*)));
        }

        m_Manager = manager;

        updateStatus();

        if (m_Manager) {
            connect(m_Manager, SIGNAL(finished(QNetworkReply*)), this, SLOT(requestFinished(QNetworkReply*)));
            if (Status_Ready == m_Status) {
                poll();
            }
        }
    }
}

void
VodModel::requestFinished(QNetworkReply* reply) {
    QMutexLocker guard(&m_Lock);
    reply->deleteLater();
    QHash<QNetworkReply*, int>::iterator it = m_PendingRequests.find(reply);

    switch (reply->error()) {
    case QNetworkReply::OperationCanceledError:
        break;
    case QNetworkReply::NoError: {
        // Get the http status code
        int v = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        if (v >= 200 && v < 300) // Success
        {
            // Here we got the final reply
            switch (it.value()) {
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
                qWarning() << "Unhandled level" << it.value();
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

            m_PendingRequests.insert(makeRequest(newUrl), it.value());
            ++m_TotalUrlsToFetched;
        } else  {
            qDebug() << "Http status code:" << v;
        }
    } break;
    default:
        qDebug() << "Network request failed: " << reply->errorString();
        break;
    }

    m_PendingRequests.erase(it);
    m_RequestStage.remove(reply);
    m_RequestMatch.remove(reply);
    m_RequestVod.remove(reply);

    ++m_CurrentUrlsToFetched;
    updateVodFetchingProgress();


    if (m_PendingRequests.isEmpty()) {
        if (m_TournamentRequestQueue.empty()) {

    //        QSqlQuery q(*m_Database);
    //        if (!q.exec("BEGIN TRANSACTION")) {
    //            setError(Error_CouldntStartTransaction);
    //            setStatus(Status_Error);
    //            return;
    //        }

    //        if (!q.exec("DELETE FROM vods")) {
    //            setError(Error_SqlTableManipError);
    //            setStatus(Status_Error);
    //            return;
    //        }

    //        insertVods();

            //QSqlQuery q(*m_Database);

            // TODO: check for busy in case of reader
    //        if (!q.exec("COMMIT TRANSACTION")) {
    //            setError(Error_CouldntEndTransaction);
    //            setStatus(Status_Error);
    //            return;
    //        }

            m_Tournaments.clear();
            Q_ASSERT(m_RequestStage.empty());
            Q_ASSERT(m_RequestMatch.empty());
            Q_ASSERT(m_RequestVod.empty());

            QSqlQuery q(*m_Database);
            if (!q.exec("select count(*) from vods") || !q.next()) {
                setError(Error_SqlTableManipError);
                setStatus(Status_Error);
                return;
            }

            qDebug() << q.value(0).toInt() << "rows";

            if (!q.exec("select distinct game from vods")) {
                setError(Error_SqlTableManipError);
                setStatus(Status_Error);
                return;
            }

            qDebug() << "games";
            while (q.next()) {
                qDebug() << q.value(0).toInt();

            }

            setStatus(Status_VodFetchingComplete);

            m_lastUpdated = QDateTime::currentDateTime();
            emit lastUpdatedChanged(m_lastUpdated);
            qDebug("poll finished");

            m_Timer.start();
        } else {
            if (m_AddedVods) {
                m_AddedVods = false;
                emit vodsAdded();
            }

            TournamentRequestData requestData = m_TournamentRequestQueue.first();
            m_TournamentRequestQueue.pop_front();

            QNetworkReply* reply = makeRequest(requestData.url);
            m_RequestStage.insert(reply, requestData.tournament);
            m_PendingRequests.insert(reply, 1);
            ++m_TotalUrlsToFetched;

            qDebug() << "fetch" << requestData.tournament->name << requestData.tournament->year << requestData.tournament->game << requestData.tournament->season;
        }
    }
}



QDateTime
VodModel::lastUpdated() const {
    return m_lastUpdated;
}

void
VodModel::parseLevel0(QNetworkReply* reply) {
    QString soup = QString::fromUtf8(reply->readAll());
    QSqlQuery q(*m_Database);
//    qDebug() << soup;
    for (int start = 0, x = 0, found = aHrefRegex.indexIn(soup, start);
         found != -1/* && x < 1*/;
         start = found + aHrefRegex.cap(0).length(), found = aHrefRegex.indexIn(soup, start), ++x) {

        QString link = aHrefRegex.cap(1);
        QString name = aHrefRegex.cap(2).trimmed();
        QString date = aHrefRegex.cap(3);

        qDebug() << link << name << date;

        Game game = Game_Sc2;
        int year = 0;
        int season = 0;
        QString fullTitle = name;
        tryGetYear(date, &year);
        tryGetSeason(name, &season);
        tryGetGame(name, &game);
        name = name.trimmed();

        bool fetch = true;
        if (QDate::currentDate().year() != year) { // current year may change
            if (!q.prepare("SELECT count(*) FROM vods WHERE tournament=? AND year=? AND game=? AND season=?")) {
                qCritical() << "failed to prepare vod select" << q.lastError();
                continue;
            }

            q.addBindValue(name);
            q.addBindValue(year);
            q.addBindValue(game);
            q.addBindValue(season);

            if (!q.exec() || !q.next()) {
                qCritical() << "failed to exec query/move to first record" << q.lastError();
                continue;
            }

            fetch = q.value(0).toInt() == 0;
        }

        if (fetch) {
            m_Tournaments.append(TournamentData());
            TournamentData& t = m_Tournaments.last();
            t.name = name;
            t.fullName = fullTitle;
            t.year = year;
            t.isShow = false;
            t.game = game;
            t.season = season;

            TournamentRequestData requestData;
            requestData.tournament = &t;
            requestData.url = link;
            m_TournamentRequestQueue.append(requestData);
        } else {
            qDebug() << "have" << name << year << game << season;
        }

    }
}

void
VodModel::parseLevel1(QNetworkReply* reply) {
    QString soup = QString::fromUtf8(reply->readAll());
//    qDebug() << soup;

    QSqlQuery q(*m_Database);
    QList< int > stageOffsets;
    TournamentData* t = m_RequestStage.value(reply, Q_NULLPTR);
    Q_ASSERT(t);

    for (int stageStart = 0, stageFound = tournamentPageStageNameRegex.indexIn(soup, stageStart), stageIndex = 0;
         stageFound != -1;
         stageStart = stageFound + tournamentPageStageNameRegex.cap(0).length(), stageFound = tournamentPageStageNameRegex.indexIn(soup, stageStart), ++stageIndex) {

        QString name = tournamentPageStageNameRegex.cap(1);


        t->stages.append(StageData());

        StageData& s = t->stages.last();
        s.parent = t;
        s.name = name;
        s.index = stageIndex;

        stageOffsets << stageFound;
    }

    stageOffsets << soup.length();

    QLinkedList<StageData>::iterator stageIterator = t->stages.begin();
    for (int i = 0; i < stageOffsets.size() - 1; ++i, ++stageIterator) {
        StageData& s = *stageIterator;
        QStringRef subString(&soup, stageOffsets[i], stageOffsets[i+1]-stageOffsets[i]);
        QString matchPart = subString.toString();
//        qDebug() << matchPart;

        for (int matchStart = 0, matchFound = tournamentPageMatchRegex.indexIn(matchPart, matchStart);
             matchFound != -1;
             matchStart = matchFound + tournamentPageMatchRegex.cap(0).length(), matchFound = tournamentPageMatchRegex.indexIn(matchPart, matchStart)) {

            QString link = tournamentPageMatchRegex.cap(1);
            QString matchOrEpisode = tournamentPageMatchRegex.cap(2);
            QString matchNumber = tournamentPageMatchRegex.cap(3);
            QString matchDate = tournamentPageMatchRegex.cap(4).trimmed();

            QStringList parts = matchDate.split(QChar('/'), QString::SkipEmptyParts);
            QDate qmatchDate(t->year, parts[0].toInt(), parts[1].toInt());
            int imatchNumber = matchNumber.toInt();

            if (!q.prepare(
"SELECT count(*) FROM vods WHERE tournament=? AND year=? AND game=? AND season=? "
"AND match_number=? AND match_date=?")) {
                qCritical() << "failed to prepare vod select" << q.lastError();
                continue;
            }

            q.addBindValue(t->name);
            q.addBindValue(t->year);
            q.addBindValue(t->game);
            q.addBindValue(t->season);
            q.addBindValue(imatchNumber);
            q.addBindValue(qmatchDate);

            if (!q.exec() || !q.next()) {
                qCritical() << "failed to exec query/move to first record" << q.lastError();
                continue;
            }

            if (q.value(0) == 0) {
                s.matches.append(MatchData());
                MatchData& m = s.matches.last();
                m.parent = &s;
                m.matchNumber = imatchNumber;
                m.matchDate = qmatchDate;
                t->isShow = matchOrEpisode.compare(QStringLiteral("match"), Qt::CaseInsensitive) == 0;

                QNetworkReply* reply = makeRequest(link);
                m_RequestVod.insert(reply, &m);
                m_PendingRequests.insert(reply, 2);
                ++m_TotalUrlsToFetched;
                qDebug() << "fetch" << t->name << t->year << t->game << t->season << imatchNumber << qmatchDate;
            } else {
                qDebug() << "have" << t->name << t->year << t->game << t->season << imatchNumber << qmatchDate;
            }
        }
    }
}

void
VodModel::parseLevel2(QNetworkReply* reply) {
    QString soup = QString::fromUtf8(reply->readAll());
    //qDebug() << soup;

    MatchData* m = m_RequestVod.value(reply, Q_NULLPTR);
    Q_ASSERT(m);
    int index = iFrameRegex.indexIn(soup);
    if (index >= 0) {
        QString url = iFrameRegex.cap(1);
        m->url = url;
        StageData* s = m->parent;
        TournamentData* t = s->parent;

        QSqlQuery q(*m_Database);
        if (!q.prepare(
"INSERT INTO vods ("
"match_date, side1, side2, url, tournament, title, season, stage_name,"
"match_number, match_count, game, year, stage_index, seen) "
"VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)")) {
            qCritical() << "failed to prepare vod insert" << q.lastError();
            return;
        }

        q.bindValue(0, m->matchDate);
        q.bindValue(1, m->side1);
        q.bindValue(2, m->side2);
        q.bindValue(3, m->url);
        q.bindValue(4, t->name);
        q.bindValue(5, t->fullName);
        q.bindValue(6, t->season);
        q.bindValue(7, s->name);
        q.bindValue(8, m->matchNumber);
        q.bindValue(9, s->matches.size());
        q.bindValue(10, t->game);
        q.bindValue(11, t->year);
        q.bindValue(12, s->index);
        q.bindValue(13, false);

        if (!q.exec()) {
            qCritical() << "failed to exec vod insert" << q.lastError();
            return;
        }

        m_AddedVods = true;
        qDebug() << "add"
                 << m->matchDate
                 << m->side1
                 << m->side2
                 << m->url
                 << t->name
                 << t->fullName
                 << t->season
                 << s->name
                 << m->matchNumber
                 << s->matches.size()
                 << t->game
                 << t->year
                 << s->index
                 << false;

    } else {
        qWarning() << "no vod url found in" << reply->request().url();
    }
}

void
VodModel::insertVods() {
    QSqlQuery q(*m_Database);
    QLinkedList<TournamentData>::const_iterator tEnd = m_Tournaments.cend();
    QLinkedList<TournamentData>::const_iterator tIt = m_Tournaments.cbegin();
    for (; tIt != tEnd; ++tIt) {
        const TournamentData& t = *tIt;
        QLinkedList<StageData>::const_iterator sEnd = t.stages.cend();
        QLinkedList<StageData>::const_iterator sIt = t.stages.cbegin();
        for (int stageIndex = 0; sIt != sEnd; ++sIt, ++stageIndex) {
            const StageData& s = *sIt;
            QLinkedList<MatchData>::const_iterator mEnd = s.matches.cend();
            QLinkedList<MatchData>::const_iterator mIt = s.matches.cbegin();
            for (; mIt != mEnd; ++mIt) {
                const MatchData& m = *mIt;
                if (!q.prepare(
    "INSERT INTO vods ("
    "match_date, side1, side2, url, tournament, title, season, stage_name,"
    "match_number, match_count, game, year, stage_index, seen) "
    "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)")) {
                    qCritical() << "failed to prepare vod insert" << q.lastError();
                    continue;
                }

                q.bindValue(0, m.matchDate);
                q.bindValue(1, m.side1);
                q.bindValue(2, m.side2);
                q.bindValue(3, m.url);
                q.bindValue(4, t.name);
                q.bindValue(5, t.fullName);
                q.bindValue(6, t.season);
                q.bindValue(7, s.name);
                q.bindValue(8, m.matchNumber);
                q.bindValue(9, s.matches.size());
                q.bindValue(10, t.game);
                q.bindValue(11, t.year);
                q.bindValue(12, stageIndex);
                q.bindValue(13, false);

                if (!q.exec()) {
                    qCritical() << "failed to exec vod insert" << q.lastError();
                    continue;
                }
            }
        }
    }
}

//void
//VodModel::parseReply(QByteArray& data) {
//    Q_ASSERT(aHrefRegex.isValid());

//    QString soup = QString::fromUtf8(data);
//    for (int start = 0, x = 0, found = aHrefRegex.indexIn(soup, start);
//         found != -1 && x < 3;
//         start = found + aHrefRegex.cap(0) .length(), found = aHrefRegex.indexIn(soup, start), ++x) {

//        QString link = aHrefRegex.cap(1);

//        qDebug() << "fetching" << BaseUrl + link;

//        QNetworkRequest request(BaseUrl + link);
//        request.setRawHeader("User-Agent", ms_UserAgent);
//        QNetworkReply* reply = m_Manager->get(request);
//       m_PendingRequests.insert(reply);
////        break;
//    }

//}

//void
//VodModel::addVods(QByteArray& data) {
//    QString soup = QString::fromUtf8(data);
//    int index = titleRegex.indexIn(soup);
//    if (index == -1) {
//        return;
//    }


//    QString title = titleRegex.cap(1);
//    title.replace(tags, QString());
//    QString fullTitle = title.trimmed();
//    title = fullTitle;
//    int year = 0;
//    int season = 0;
//    Game game = Game_Sc2;
//    tryGetYear(title, &year);
//    tryGetSeason(title, &season);
//    tryGetGame(title, &game);
//    title = title.trimmed();



//    QList<QVariantList> args;
//    args.reserve(32);

//    for (int stageIndex = 0, stageStart = 0, stageFound = stageRegex.indexIn(soup, stageStart);
//         stageFound != -1;
//         stageStart = stageFound + stageRegex.cap(0).length(),
//            stageFound = stageRegex.indexIn(soup, stageStart),
//            ++stageIndex) {

//        QString stageTitle = stageRegex.cap(1);
//        stageTitle = stageTitle.remove(tags).trimmed();

//        QString stageData = stageRegex.cap(2);
//        //qDebug() << stageTitle << stageData;

//        args.clear();
//        int matchNumber = 1;
//        for (int matchStart = 0, matchFound = tournamentPageMatchRegex.indexIn(stageData, matchStart);
//             matchFound != -1;
//             matchStart = matchFound + tournamentPageMatchRegex.cap(0).length(), matchFound = tournamentPageMatchRegex.indexIn(stageData, matchStart), ++matchNumber) {

//            QString matchNumberString = tournamentPageMatchRegex.cap(2);
//            QString junk = tournamentPageMatchRegex.cap(3);
//            junk.remove(tags);

//            junk.replace(QStringLiteral("&nbsp;"), QStringLiteral(" "));


//            QDateTime date;
//            tryGetDate(junk, &date);

//            junk.replace(QStringLiteral("reveal match"), QString(), Qt::CaseInsensitive);
//            junk.replace(QStringLiteral("reveal episode"), QString(), Qt::CaseInsensitive);
//            QStringList sides = junk.split(QStringLiteral("vs"));

//            args.push_back(QVariantList());
//            QVariantList& arg = args.last();
//            arg.reserve(6);
//            arg << matchNumber
//                << date
//                << (sides.size() == 2 ? sides[0].trimmed() : QString())
//                << (sides.size() == 2 ? sides[1].trimmed() : QString())
//                << tournamentPageMatchRegex.cap(1)
//                << stageIndex;

//        }

//        // fix match numbers
//        for (int i = 1; i < matchNumber; ++i) {

//            QVariantList& arg = args[i-1];


//            QSqlQuery q(*m_Database);
//            if (!q.prepare(
//"INSERT INTO vods ("
//"match_date, side1, side2, url, tournament, title, season, stage_name,"
//"match_number, match_count, game, year, stage_index) "
//"VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)")) {
//                qCritical() << "failed to prepare vod insert" << q.lastError();
//                continue;
//            }

//            q.bindValue(0, arg[1]);
//            q.bindValue(1, arg[2]);
//            q.bindValue(2, arg[3]);
//            q.bindValue(3, arg[4]);
//            q.bindValue(4, title);
//            q.bindValue(5, fullTitle);
//            q.bindValue(6, season);
//            q.bindValue(7, stageTitle);
//            q.bindValue(8, arg[0]);
//            q.bindValue(9, matchNumber-1);
//            q.bindValue(10, game);
//            q.bindValue(11, year);
//            q.bindValue(12, arg[5]);
//            q.bindValue(13, false);

//            if (!q.exec()) {
//                qCritical() << "failed to exec vod insert" << q.lastError();
//                continue;
//            }
//        }
//    }
//}

bool
VodModel::tryGetYear(QString& inoutSrc, int* year) {
    Q_ASSERT(fuzzyYearRegex.isValid());
    int i = fuzzyYearRegex.indexIn(inoutSrc);
    if (i >= 0) {
        QString match = fuzzyYearRegex.cap(1);
        bool ok = false;
        int result = match.toInt(&ok);
        if (ok) {
            *year = result;
            inoutSrc.remove(yearRegex);
            return true;
        }
    }

    return false;
}

bool
VodModel::tryGetSeason(QString& inoutSrc, int* season) {
    for (size_t i = 0; i < _countof(EnglishWordSeasons); ++i) {
        int index = inoutSrc.indexOf(EnglishWordSeasons[i], 0, Qt::CaseInsensitive);
        if (index >= 0) {
            *season = i + 1;
            inoutSrc.remove(index, EnglishWordSeasons[i].size());
            return true;
        }
    }

    for (size_t i = 0; i < 20; ++i) {
        QString romanNumeral = romanNumeralFor(i + 1);
        int index = inoutSrc.indexOf(romanNumeral, 0, Qt::CaseInsensitive);
        if (index >= 0 && romanNumeral.length() != inoutSrc.length()) {
            bool extract = false;
            // at end? then there must be whitespace in front of the numeral
            if (index + romanNumeral.length() == inoutSrc.length() &&
                inoutSrc[index-1].isSpace()) {
                extract = true;
            // at begin? then ws must be following
            } else if (index == 0 && inoutSrc[romanNumeral.length()].isSpace()) {
                extract = true;
            } else { // must be surrounded by ws
                extract = inoutSrc[index + romanNumeral.length()].isSpace() &&
                        inoutSrc[index-1].isSpace();
            }

            if (extract) {
                *season = i + 1;
                inoutSrc.remove(index, romanNumeral.size());
            }

            return true;
        }

        QString shortSeasonString = QStringLiteral("s%1").arg(i+1);
        index = inoutSrc.indexOf(shortSeasonString, 0, Qt::CaseInsensitive);
        if (index >= 0) {
            *season = i + 1;
            inoutSrc.remove(index, shortSeasonString.size());
            return true;
        }
    }

    return false;
}

bool
VodModel::tryGetIcon(const QString& title, QString* iconPath) {
    QString lower = title.toLower();
    const TournamentToIconMapType::const_iterator it = TournamentToIconMap.find(lower);
    if (it == TournamentToIconMap.end()) {
        return false;
    }

    *iconPath = it.value();

    return true;
}

bool
VodModel::tryGetDate(QString& inoutSrc, QDateTime* date) {
    int i = dateRegex.indexIn(inoutSrc);
    if (i >= 0) {
        QStringRef subString(&inoutSrc, i, 10);
        *date = QDateTime::fromString(subString.toString(), QStringLiteral("yyyy-MM-dd"));
        inoutSrc.remove(i, 10);
        return true;
    }

    return false;
}

bool
VodModel::tryGetGame(QString& inoutSrc, Game* game) {
    int index = inoutSrc.indexOf(QStringLiteral("(bw)"), 0, Qt::CaseInsensitive);
    if (index >= 0) {
        inoutSrc.remove(index, 4);
        *game = Game_Broodwar;
        return true;
    }

    return false;
}
void
VodModel::setDatabase(QSqlDatabase* db) {
    m_Database = db;

    updateStatus();

    if (m_Database) {
        createTablesIfNecessary();
        if (Status_Ready == m_Status) {
            poll();
        }
    }
}

void
VodModel::createTablesIfNecessary() {
    QSqlQuery q(*m_Database);

    static char const* const queries[] = {
        "CREATE TABLE IF NOT EXISTS vods ("
        "    match_date INTEGER,          "
        "    side1 TEXT,                  "
        "    side2 TEXT,                  "
        "    url TEXT,                    "
        "    tournament TEXT,             "
        "    title TEXT,                  "
        "    season INTEGER,              "
        "    stage_name TEXT,             "
        "    match_number INTEGER,        "
        "    match_count INTEGER,         "
        "    game INTEGER,                "
        "    year INTEGER,                "
        "    stage_index INTEGER,         "
        "    seen INTEGER                 "
        ")                                ",
    };

    for (size_t i = 0; i < _countof(queries); ++i) {
        if (!q.prepare(queries[i]) || !q.exec()) {
            qCritical() << "failed to create vods table" << q.lastError();
            setError(Error_CouldntCreateSqlTables);
            setStatus(Status_Error);
        }
    }
}

void
VodModel::setStatus(Status status) {
    if (m_Status != status) {
        m_Status = status;
        emit statusChanged();
    }
}

void
VodModel::setError(Error error) {
    if (m_Error != error) {
        m_Error = error;
        emit errorChanged();
        emit errorStringChanged(errorString());
    }
}

QString
VodModel::errorString() const {
    switch (m_Error) {
    case Error_None:
        return QString();
    case Error_CouldntCreateSqlTables:
        return tr("couldn't create database tables");
    default:
        return tr("dunno what happened TT");
    }
}

void
VodModel::updateStatus() {
    if (m_Status == Status_Uninitialized) {
        if (m_Database && m_Manager) {
            QSqlQuery q(*m_Database);

            if (q.exec("SELECT name FROM sqlite_master WHERE type=table AND name=vods") && q.next()) {
                if (q.exec("SELECT COUNT(*) as count from vods") && q.next()) {
                    //int i = q.record().indexOf("count");
                    //QVariant value = q.value(i);
                    QVariant value = q.value(0);
                    if (value.toInt() > 0) {
                        setStatus(Status_VodFetchingComplete);
                    } else {
                        setStatus(Status_Ready);
                    }
                } else {
                    setStatus(Status_Error);
                    setError(Error_SqlTableManipError);
                }
            } else {
                setStatus(Status_Ready);
            }
        }
    } else {
        if (!m_Database || ! m_Manager) {
            setStatus(Status_Uninitialized);
//            setError(Error_None);
        }
    }
}

QString
VodModel::mediaPath(const QString& str) const {
    return str;
}

QString
VodModel::dataDir() const {
    return QStringLiteral(QT_STRINGIFY(SAILFISH_DATADIR));
}

QString
VodModel::label(const QString& key, const QVariant& value) const {
    if (value.isValid()) {
        if (key == QStringLiteral("game")) {
            int game = value.toInt();
            switch (game) {
            case Game_Broodwar:
                return tr("Broodwar");
            case Game_Sc2:
                return tr("StarCraft II");
            default:
                return tr("unknown game");
            }
        } else if (key == QStringLiteral("year")) {
            return value.toString();
        } else if (key == QStringLiteral("tournament")) {
            return value.toString();
        }

        return value.toString();
    }

    return tr(key.toLocal8Bit().data());
}

QString
VodModel::icon(const QString& key, const QVariant& value) const {
    if (key == QStringLiteral("game")) {
        int game = value.toInt();
        switch (game) {
        case Game_Broodwar:
            return QStringLiteral(QT_STRINGIFY(SAILFISH_DATADIR) "/media/bw.png");
        case Game_Sc2:
            return QStringLiteral(QT_STRINGIFY(SAILFISH_DATADIR) "/media/sc2.png");
        default:
            return QStringLiteral(QT_STRINGIFY(SAILFISH_DATADIR) "/media/game.png");
        }
    }

    //return QStringLiteral(QT_STRINGIFY(SAILFISH_DATADIR) "/media/default.png");
    return QStringLiteral("image://theme/icon-m-sailfish");

}

qreal
VodModel::vodFetchingProgress() const {
    return m_VodFetchingProgress;
}

void
VodModel::setVodFetchingProgress(qreal value) {
    if (value != m_VodFetchingProgress) {
        m_VodFetchingProgress = value;
        emit vodFetchingProgressChanged(m_VodFetchingProgress);
    }
}

void
VodModel::updateVodFetchingProgress() {
    if (m_TotalUrlsToFetched) {
        qreal progress = m_CurrentUrlsToFetched;
        progress /= m_TotalUrlsToFetched;
        setVodFetchingProgress(progress);
    } else {
        setVodFetchingProgress(0);
    }
}
