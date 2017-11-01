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


const QString BaseUrl = QStringLiteral("https://sc2links.com/");
const QString EntryUrl = QStringLiteral("https://sc2links.com/tournament.php");


const QRegExp aHrefRegex(QStringLiteral("<a\\s+href=[\"'](tournament.php\\?tournament=[^\"']+)[\"'][^>]*>"), Qt::CaseInsensitive);
const QRegExp dateRegex(QStringLiteral("\\d{4}-\\d{2}-\\d{2}"));
const QRegExp fuzzyYearRegex(QStringLiteral("(?:\\s*\\S+\\s+|\\s*)(\\d{4})(?:\\s*|\\s+\\S+\\s*)"));
const QRegExp yearRegex(QStringLiteral("\\d{4}"));
// <h2 style="padding:0;color:#DB5C04;"><b>Afreeca Starleague S4 (BW) 2017</b></h2>
const QRegExp titleRegex("<h2[^>]*><b>(.*)</b></h2>");
const QRegExp tags(QStringLiteral("<[^>]+>"));
const QRegExp stageRegex(QStringLiteral("<table[^>]*><p\\s+class=['\"]head_title['\"][^>]*>(.*)</p>(.*)</table>"));

/// <tr><td><a href='https://youtu.be/toURJRI
// <tr><td><a href='https://youtu.be/e9eCGElQJIM?t=817' target='_blank' style='padding:5px 0 5px 5px;width:175px;float:left;'> Match 1</a>&nbsp;<span style='padding:5px;float:right;'>2017-10-15</span>&nbsp;&nbsp; <span id='rall'><a href='javascript:void(0);' id='handover'>Reveal Match</a><span class='activespan'>&nbsp;<span class='r1'><img width='9' height='9' src='images/box1.jpg'>&nbsp;<b>Larva</b></span><span class='r2'> vs </span><span class='r3'><img width='9' height='9' src='images/box2.jpg'>&nbsp;<b>Rain</b></span></span></span></td></tr>
const QRegExp matchRegex(QStringLiteral("<tr><td><a href=['\"]([^'\"]+)['\"][^>]*>\\D+(\\d+)\\s*</a>(.*)</td></tr>"));

typedef QHash<QString, QString> TournamentToIconMapType;
const TournamentToIconMapType TournamentToIconMap;

int InitializeStatics() {
    Q_ASSERT(aHrefRegex.isValid());
    Q_ASSERT(dateRegex.isValid());
    Q_ASSERT(yearRegex.isValid());
    Q_ASSERT(titleRegex.isValid());
    Q_ASSERT(tags.isValid());
    Q_ASSERT(stageRegex.isValid());
    Q_ASSERT(matchRegex.isValid());

    const_cast<QRegExp&>(titleRegex).setMinimal(true);
    const_cast<QRegExp&>(tags).setMinimal(true);
    const_cast<QRegExp&>(stageRegex).setMinimal(true);
    const_cast<QRegExp&>(matchRegex).setMinimal(true);

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
        qWarning("Poll in progress");
        return;
    default:
        qWarning() << "Error: " << qPrintable(errorString());
        return;
    }

    QMutexLocker guard(&m_Lock);

    QSqlQuery q(*m_Database);
    if (!q.exec("BEGIN TRANSACTION")) {
        setError(Error_CouldntStartTransaction);
        setStatus(Status_Error);
        return;
    }

    if (!q.exec("DELETE FROM vods")) {
        setError(Error_SqlTableManipError);
        setStatus(Status_Error);
        return;
    }

    QNetworkRequest request;
    request.setUrl(QUrl(EntryUrl));
    request.setRawHeader("User-Agent", ms_UserAgent);// must be set else no reply

    QNetworkReply* reply = m_Manager->get(request);
    m_PendingRequests.insert(reply);
    setStatus(Status_VodFetchingInProgress);
    qDebug("poll started");
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

    switch (reply->error()) {
    case QNetworkReply::OperationCanceledError:
        break;
    case QNetworkReply::NoError: {
        // Get the http status code
        int v = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        if (v >= 200 && v < 300) // Success
        {
            // Here we got the final reply
            QByteArray content = reply->readAll();
            if (reply->request().url() == QUrl(EntryUrl)) {
                parseReply(content);
            } else {
                addVods(content);
            }
        }
        else if (v >= 300 && v < 400) // Redirection
        {
            // Get the redirection url
            QUrl newUrl = reply->attribute(QNetworkRequest::RedirectionTargetAttribute).toUrl();
            // Because the redirection url can be relative,
            // we have to use the previous one to resolve it
            newUrl = reply->url().resolved(newUrl);

            QNetworkAccessManager *manager = reply->manager();
            QNetworkRequest redirection(newUrl);
            m_PendingRequests.insert(manager->get(redirection));
        } else  {
            qDebug() << "Http status code:" << v;
        }
    } break;
    default:
        qDebug() << "Network request failed: " << reply->errorString();
        break;
    }

    if (m_PendingRequests.remove(reply)) {
        if (m_PendingRequests.isEmpty()) {

            QSqlQuery q(*m_Database);
            if (!q.exec("COMMIT TRANSACTION")) {
                setError(Error_CouldntEndTransaction);
                setStatus(Status_Error);
                return;
            }

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

            //m_Database->close();
            //m_Database->open();


            setStatus(Status_VodFetchingComplete);

            m_lastUpdated = QDateTime::currentDateTime();
            emit lastUpdatedChanged(m_lastUpdated);
            qDebug("poll finished");

            m_Timer.start();
        }
    }
}



QDateTime
VodModel::lastUpdated() const {
    return m_lastUpdated;
}

void
VodModel::parseReply(QByteArray& data) {
    Q_ASSERT(aHrefRegex.isValid());

    QString soup = QString::fromUtf8(data);
    for (int start = 0, x = 0, found = aHrefRegex.indexIn(soup, start);
         found != -1 && x < 3;
         start = found + aHrefRegex.cap(0) .length(), found = aHrefRegex.indexIn(soup, start), ++x) {

        QString link = aHrefRegex.cap(1);

        qDebug() << "fetching" << BaseUrl + link;

        QNetworkRequest request(BaseUrl + link);
        request.setRawHeader("User-Agent", ms_UserAgent);
        QNetworkReply* reply = m_Manager->get(request);
       m_PendingRequests.insert(reply);
//        break;
    }

}

void
VodModel::addVods(QByteArray& data) {
    QString soup = QString::fromUtf8(data);
    int index = titleRegex.indexIn(soup);
    if (index == -1) {
        return;
    }


    QString title = titleRegex.cap(1);
    title.replace(tags, QString());
    QString fullTitle = title.trimmed();
    title = fullTitle;
    int year = 0;
    int season = 0;
    Game game = Game_Sc2;
    tryGetYear(title, &year);
    tryGetSeason(title, &season);
    tryGetGame(title, &game);
    title = title.trimmed();



    QList<QVariantList> args;
    args.reserve(32);

    for (int stageIndex = 0, stageStart = 0, stageFound = stageRegex.indexIn(soup, stageStart);
         stageFound != -1;
         stageStart = stageFound + stageRegex.cap(0).length(),
            stageFound = stageRegex.indexIn(soup, stageStart),
            ++stageIndex) {

        QString stageTitle = stageRegex.cap(1);
        stageTitle = stageTitle.remove(tags).trimmed();

        QString stageData = stageRegex.cap(2);
        //qDebug() << stageTitle << stageData;

        args.clear();
        int matchNumber = 1;
        for (int matchStart = 0, matchFound = matchRegex.indexIn(stageData, matchStart);
             matchFound != -1;
             matchStart = matchFound + matchRegex.cap(0).length(), matchFound = matchRegex.indexIn(stageData, matchStart), ++matchNumber) {

            QString matchNumberString = matchRegex.cap(2);
            QString junk = matchRegex.cap(3);
            junk.remove(tags);

            junk.replace(QStringLiteral("&nbsp;"), QStringLiteral(" "));


            QDateTime date;
            tryGetDate(junk, &date);

            junk.replace(QStringLiteral("reveal match"), QString(), Qt::CaseInsensitive);
            junk.replace(QStringLiteral("reveal episode"), QString(), Qt::CaseInsensitive);
            QStringList sides = junk.split(QStringLiteral("vs"));

            args.push_back(QVariantList());
            QVariantList& arg = args.last();
            arg.reserve(6);
            arg << matchNumber
                << date
                << (sides.size() == 2 ? sides[0].trimmed() : QString())
                << (sides.size() == 2 ? sides[1].trimmed() : QString())
                << matchRegex.cap(1)
                << stageIndex;

        }

        // fix match numbers
        for (int i = 1; i < matchNumber; ++i) {

            QVariantList& arg = args[i-1];


            QSqlQuery q(*m_Database);
            if (!q.prepare(
"INSERT INTO vods ("
"match_date, side1, side2, url, tournament, title, season, stage_name,"
"match_number, match_count, game, year, stage_index) "
"VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)")) {
                qCritical() << "failed to prepare vod insert" << q.lastError();
                continue;
            }

            q.bindValue(0, arg[1]);
            q.bindValue(1, arg[2]);
            q.bindValue(2, arg[3]);
            q.bindValue(3, arg[4]);
            q.bindValue(4, title);
            q.bindValue(5, fullTitle);
            q.bindValue(6, season);
            q.bindValue(7, stageTitle);
            q.bindValue(8, arg[0]);
            q.bindValue(9, matchNumber-1);
            q.bindValue(10, game);
            q.bindValue(11, year);
            q.bindValue(12, arg[5]);

            if (!q.exec()) {
                qCritical() << "failed to exec vod insert" << q.lastError();
                continue;
            }
        }
    }
}

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
        if (index >= 0) {
            *season = i + 1;
            inoutSrc.remove(index, romanNumeral.size());
            return true;
        }

        QString shortSeasonString = QStringLiteral("s") + QString::number(i+1);
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
        "    stage_index INTEGER          "
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
