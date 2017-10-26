#include "Sc2LinksDotCom.h"
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QRegExp>
#include <QMutexLocker>
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
/*VodDate,
        VodSide1, // Taeja, Poland
        VodSide2, // MVP, France
        VodUrl, //
        VodTournament, // GSL, Homestory Cup
        VodSeason,  // Season 1, XII
        VodStage, // RO16, Day 1, etc
        VodMatchNumber, // Match in best of X
        VodMatchCount, // Best of X
        */




const char *VodStrings[] = {
    QT_TR_NOOP("date"),
    QT_TR_NOOP("side1"),
    QT_TR_NOOP("side2"),
    QT_TR_NOOP("url"),
    QT_TR_NOOP("tournament"),
    QT_TR_NOOP("season"),
    QT_TR_NOOP("stage"),
    QT_TR_NOOP("match_number"),
    QT_TR_NOOP("match_count"),
    QT_TR_NOOP("game"),
    QT_TR_NOOP("icon"),
};

const QLatin1String EnglishWordSeasons[] = {
    QLatin1String("season one"),
    QLatin1String("season two"),
    QLatin1String("season three"),
    QLatin1String("season four"),
    QLatin1String("season five"),
    QLatin1String("season six"),
    QLatin1String("season seven"),
    QLatin1String("season eight"),
    QLatin1String("season nine"),
    QLatin1String("season ten"),
};

const QLatin1String GameIcons[] = {
    QLatin1String("bw.png"),
    QLatin1String("sc2.png"),
};

const QString BaseUrl = QLatin1String("https://sc2links.com/");
const QString EntryUrl = QLatin1String("https://sc2links.com/tournament.php");


const QRegExp aHrefRegex(QLatin1String("<a\\s+href=[\"'](tournament.php\\?tournament=[^\"']+)[\"'][^>]*>"), Qt::CaseInsensitive);
const QRegExp dateRegex(QLatin1String("\\d{4}-\\d{2}-\\d{2}"));
const QRegExp fuzzyYearRegex(QLatin1String("(?:\\s*\\S+\\s+|\\s*)(\\d{4})(?:\\s*|\\s+\\S+\\s*)"));
const QRegExp yearRegex(QLatin1String("\\d{4}"));
// <h2 style="padding:0;color:#DB5C04;"><b>Afreeca Starleague S4 (BW) 2017</b></h2>
const QRegExp titleRegex("<h2[^>]*><b>(.*)</b></h2>");
const QRegExp tags(QLatin1String("<[^>]+>"));
const QRegExp stageRegex(QLatin1String("<table[^>]*><p\\s+class=['\"]head_title['\"][^>]*>(.*)</p>(.*)</table>"));

/// <tr><td><a href='https://youtu.be/toURJRI
// <tr><td><a href='https://youtu.be/e9eCGElQJIM?t=817' target='_blank' style='padding:5px 0 5px 5px;width:175px;float:left;'> Match 1</a>&nbsp;<span style='padding:5px;float:right;'>2017-10-15</span>&nbsp;&nbsp; <span id='rall'><a href='javascript:void(0);' id='handover'>Reveal Match</a><span class='activespan'>&nbsp;<span class='r1'><img width='9' height='9' src='images/box1.jpg'>&nbsp;<b>Larva</b></span><span class='r2'> vs </span><span class='r3'><img width='9' height='9' src='images/box2.jpg'>&nbsp;<b>Rain</b></span></span></span></td></tr>
const QRegExp matchRegex(QLatin1String("<tr><td><a href=['\"]([^'\"]+)['\"][^>]*>\\s*Match\\s+(\\d+)</a>(.*)</td></tr>"));

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

Vod::~Vod() {

}

Vod::Vod(int index, VodModel* parent)
    : QObject(parent)
    , m_Index(index) {

}

const VodModel*
Vod::model() const {
    return static_cast<const VodModel*>(parent());
}

QDateTime Vod::played() const {
    return model()->played(m_Index);
}
QString Vod::side1() const{
    return model()->side1(m_Index);
}
QString Vod::side2() const{
    return model()->side2(m_Index);
}
QUrl Vod::url() const{
    return model()->url(m_Index);
}
QString Vod::tournament() const{
    return model()->tournament(m_Index);
}
int Vod::season() const{
    return model()->season(m_Index);
}
QString Vod::stage() const{
    return model()->stage(m_Index);
}
int Vod::matchNumber() const{
    return model()->matchNumber(m_Index);
}
int Vod::matchCount() const {
    return model()->matchCount(m_Index);
}
Game::GameType Vod::game() const {
    return model()->game(m_Index);
}
QString Vod::icon() const{
    return model()->icon(m_Index);
}
//-----------------------------------------------------------------------------------

int
VodModel::SoaItem::size() const {
    return url.size();
}

void
VodModel::SoaItem::clear() {
    played.clear();
    side1.clear();
    side2.clear();
    url.clear();
    tournament.clear();
    season.clear();
    stage.clear();
    matchNumber.clear();
    matchCount.clear();
    game.clear();
    year.clear();
    icon.clear();
}

//-----------------------------------------------------------------------------------

const QByteArray VodModel::ms_UserAgent("MyAppName/1.0 (Nokia; Qt)");
const QHash<int, QByteArray> VodModel::ms_RoleNames = VodModel::makeRoleNames();

VodModel::~VodModel() {
    setNetworkAccessManager(Q_NULLPTR);
}

VodModel::VodModel(QObject *parent)
    //: QAbstractTableModel(parent)
//    : QObject(parent)
    //: QAbstractListModel(parent)
    : QAbstractItemModel(parent)
    , m_Lock(QMutex::Recursive) {

    m_Manager = Q_NULLPTR;
    m_Status = Uninitialized;

    connect(&m_Timer, SIGNAL(timeout()), this, SLOT(poll()));
    m_Timer.setInterval(30000);
    m_Timer.setTimerType(Qt::CoarseTimer);


//    m_FrontVodList.store(m_BackVodLists);
    m_FronIndex = 0;
}

void
VodModel::poll() {
    if (!m_Manager) {
        qWarning("Network access manager not set");
        return;
    }

    QMutexLocker guard(&m_Lock);

    if (!m_PendingRequests.isEmpty()) {
        return;
    }

//    m_Timer.stop();
//    m_Redirects.clear();
//    if (m_Reply) {
//        m_Reply->abort();
//        m_Reply->deleteLater();
//        m_Reply = Q_NULLPTR;
//    }

//    //m_Reply->deleteLater();

//    QNetworkReply* reply;
//    foreach (reply, m_RequestToTournamentMap.keys()) {
//        reply->abort();
//        reply->deleteLater();
//    }

//    m_RequestToTournamentMap.clear();
//    m_PendingTournamentRequests

    m_BackItems[(m_FronIndex + 1) % 2].clear();

    QNetworkRequest request;
    request.setUrl(QUrl(EntryUrl));
    request.setRawHeader("User-Agent", ms_UserAgent);// must be set else no reply

    QNetworkReply* reply = m_Manager->get(request);
    m_PendingRequests.insert(reply);
    m_Status = VodFetchingInProgress;
    emit statusChanged(m_Status);
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

        if (m_Manager) {
            connect(m_Manager, SIGNAL(finished(QNetworkReply*)), this, SLOT(requestFinished(QNetworkReply*)));
            poll();
        }
    }
}

void
VodModel::requestFinished(QNetworkReply* reply) {
    QMutexLocker guard(&m_Lock);
    reply->deleteLater();
//    if (reply->attribute(AbortedAttribute).toBool()) {
//        return;
//    }
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

            int previousIndex = m_FronIndex;
            int newIndex = (previousIndex + 1) % 2;


            SoaItem& previous = m_BackItems[previousIndex];
            SoaItem& current = m_BackItems[newIndex];
            int previousSize = previous.size();
            int currentSize = current.size();
            int diff = currentSize - previousSize;
            bool changed = true;
            if (diff > 0) {
                changed = previousSize > 0;
                beginInsertRows(QModelIndex(), previousSize, currentSize-1);
                m_FronIndex = newIndex;
                endInsertRows();
            } else if (diff == 0) {
                changed = previousSize > 0;
                m_FronIndex = newIndex;
            } else {
                beginRemoveRows(QModelIndex(), currentSize, previousSize-1);
                m_FronIndex = newIndex;
                endRemoveRows();
            }

            if (changed) {
                emit dataChanged(createIndex(0, 0, Q_NULLPTR), createIndex(currentSize-1, (int)_countof(VodStrings)-1, Q_NULLPTR));
            }

            m_Status = VodFetchingComplete;
            emit statusChanged(m_Status);

            m_lastUpdated = QDateTime::currentDateTime();
            emit lastUpdatedChanged(m_lastUpdated);
            qDebug("poll finished");

            m_Timer.start();
        }
    }
}



int
VodModel::rowCount(const QModelIndex &parent) const {
//    Q_UNUSED(parent);
    if (parent.isValid()) {
        return 0;
    }

    const SoaItem& item = m_BackItems[m_FronIndex.load()];
    return item.size();
}

int
VodModel::columnCount(const QModelIndex &parent) const {
    Q_UNUSED(parent);
    return (int)_countof(VodStrings);
}

QVariant
VodModel::data(const QModelIndex &index, int role) const {
    const SoaItem& item = m_BackItems[m_FronIndex.load()];

    if (!index.isValid() || index.column() >= (int)_countof(VodStrings) ||
        index.row() >= item.size()) {
        return QVariant();
    }

    int c = 0;
    if (role >= Qt::UserRole + 1) {
        c = role - Qt::UserRole - 1;
    }

    if (c >= (int)_countof(VodStrings)) {
        return QVariant();
    }

    int r = index.row();
    switch (c) {
    case VodDate:
        return item.played[r];
    case VodSide1:
        return item.side1[r];
    case VodSide2:
        return item.side2[r];
    case VodUrl:
        return item.url[r];
    case VodTournament:
        return item.tournament[r];
    case VodSeason:
        return item.season[r];
    case VodStage:
        return item.stage[r];
    case VodMatchNumber:
        return item.matchNumber[r];
    case VodMatchCount:
        return item.matchCount[r];
    case VodGame:
        return item.game[r];
    case VodIcon:
        return item.icon[r];
    default:
        break;
    }

    return QVariant();
}

QVariant
VodModel::headerData(int section, Qt::Orientation orientation, int role) const {
    if (orientation == Qt::Horizontal) {
        if (section >= 0 && section <= (int)_countof(VodStrings)) {
            return QString(VodStrings[section]);
        }
    }

    return QVariant();
}

QDateTime
VodModel::lastUpdated() const {
    return QDateTime();
}

QDateTime
VodModel::played(int index) const {
    const SoaItem& item = m_BackItems[m_FronIndex.load()];
    if (index < 0 || index >= item.size()) {
        return QDateTime();
    }

    return item.played[index];
}
QString
VodModel::side1(int index) const {
    const SoaItem& item = m_BackItems[m_FronIndex.load()];
    if (index < 0 || index >= item.size()) {
        return QString();
    }

    return item.side1[index];
}
QString
VodModel::side2(int index) const {
    const SoaItem& item = m_BackItems[m_FronIndex.load()];
    if (index < 0 || index >= item.size()) {
        return QString();
    }

    return item.side2[index];
}

QUrl
VodModel::url(int index) const {
    const SoaItem& item = m_BackItems[m_FronIndex.load()];
    if (index < 0 || index >= item.size()) {
        return QString();
    }

    return item.url[index];
}

QString
VodModel::tournament(int index) const {
    const SoaItem& item = m_BackItems[m_FronIndex.load()];
    if (index < 0 || index >= item.size()) {
        return QString();
    }

    return item.tournament[index];
}
int
VodModel::season(int index) const {
    const SoaItem& item = m_BackItems[m_FronIndex.load()];
    if (index < 0 || index >= item.size()) {
        return 0;
    }

    return item.season[index];
}
QString
VodModel::stage(int index) const {
    const SoaItem& item = m_BackItems[m_FronIndex.load()];
    if (index < 0 || index >= item.size()) {
        return QString();
    }

    return item.stage[index];
}
int
VodModel::matchNumber(int index) const {
    const SoaItem& item = m_BackItems[m_FronIndex.load()];
    if (index < 0 || index >= item.size()) {
        return 0;
    }

    return item.matchNumber[index];
}
int
VodModel::matchCount(int index) const {
    const SoaItem& item = m_BackItems[m_FronIndex.load()];
    if (index < 0 || index >= item.size()) {
        return 0;
    }

    return item.matchCount[index];
}
Game::GameType
VodModel::game(int index) const {
    const SoaItem& item = m_BackItems[m_FronIndex.load()];
    if (index < 0 || index >= item.size()) {
        return Game::Unknown;
    }

    return item.game[index];
}
QString
VodModel::icon(int index) const {
    const SoaItem& item = m_BackItems[m_FronIndex.load()];
    if (index < 0 || index >= item.size()) {
        return QString();
    }

    return item.icon[index];
}

void
VodModel::parseReply(QByteArray& data) {
    Q_ASSERT(aHrefRegex.isValid());

    QString soup = QString::fromUtf8(data);
    for (int start = 0, found = aHrefRegex.indexIn(soup, start);
         found != -1;
         start = found + aHrefRegex.cap(0) .length(), found = aHrefRegex.indexIn(soup, start)) {

        QString link = aHrefRegex.cap(1);

//        QString content = aHrefRegex.cap(2);
//        QString cleaned = content;
//        cleaned.replace(tags, QString());
//        cleaned = cleaned.trimmed();
//        int year = 0;
//        int firstSpace = cleaned.indexOf(QChar(' '));
//        if (firstSpace >= 0) {
//            QString firstPart = cleaned.left(firstSpace);
//            bool ok = false;
//            int i = firstPart.toInt(&ok);
//            if (ok) {
//                year = i;
//                cleaned = cleaned.right(cleaned.length()-firstSpace).trimmed();
//            }
//        }
//        qDebug() << link << content << year << cleaned;

        QNetworkRequest request(BaseUrl + link);
        request.setRawHeader("User-Agent", ms_UserAgent);
        QNetworkReply* reply = m_Manager->get(request);
//        reply->setAttribute(QNetworkRequest::User, QVariant(targetPtr));
        //m_RequestToTournamentMap.insert(m_Manager->get(request), index);
        m_PendingRequests.insert(reply);
        break;
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
    QString icon;
    Game::GameType game = Game::Sc2;
    tryGetYear(title, &year);
    tryGetSeason(title, &season);
    tryGetGame(title, &game);
    title = title.trimmed();
    if (!tryGetIcon(title, &icon)) {
        icon = GameIcons[game];
    }

    SoaItem& soaItem = m_BackItems[(m_FronIndex.load() + 1) % 2];

    for (int stageStart = 0, stageFound = stageRegex.indexIn(soup, stageStart);
         stageFound != -1;
         stageStart = stageFound + stageRegex.cap(0).length(), stageFound = stageRegex.indexIn(soup, stageStart)) {

        QString stageTitle = stageRegex.cap(1);
        stageTitle = stageTitle.remove(tags).trimmed();

        QString stageData = stageRegex.cap(2);
//        qDebug() << stageTitle << stageData;
        int matchNumber = 1;
        for (int matchStart = 0, matchFound = matchRegex.indexIn(stageData, matchStart);
             matchFound != -1;
             matchStart = matchFound + matchRegex.cap(0).length(), matchFound = matchRegex.indexIn(stageData, matchStart)) {

            QString matchNumberString = matchRegex.cap(2);
            QString junk = matchRegex.cap(3);
            junk.remove(tags);

            junk.replace(QLatin1String("&nbsp;"), QLatin1String(" "));


            QDateTime date;
            tryGetDate(junk, &date);

            junk.replace(QLatin1String("reveal match"), QString(), Qt::CaseInsensitive);
            QStringList sides = junk.split(QLatin1String("vs"));

//            target.push_back(Item());

//            Item& item = target.last();
//            item.game = game;
//            item.matchCount = 0;
//            item.matchNumber = matchNumber++;
//            item.played = date;
//            item.season = season;
//            item.side1 = sides.size() == 2 ? sides[0].trimmed() : QString();
//            item.side2 = sides.size() == 2 ? sides[1].trimmed() : QString();
//            item.stage = stageTitle;
//            item.tournament = title;
//            item.url = matchRegex.cap(1);
//            item.year = year;
            soaItem.game.append(game);
            soaItem.icon.append(icon);
            soaItem.matchNumber.append(matchNumber++);
            soaItem.played.append(date);
            soaItem.season.append(season);
            soaItem.side1.append(sides.size() == 2 ? sides[0].trimmed() : QString());
            soaItem.side2.append(sides.size() == 2 ? sides[1].trimmed() : QString());
            soaItem.stage.append(stageTitle);
            soaItem.tournament.append(title);
            soaItem.url.append(matchRegex.cap(1));
            soaItem.year.append(year);
        }

        // fix match numbers
        for (int i = 1; i < matchNumber; ++i) {
            //target[target.size()-i].matchCount = matchNumber;
            soaItem.matchCount.append(matchNumber);
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
        *date = QDateTime::fromString(subString.toString(), QLatin1String("yyyy-MM-dd"));
        inoutSrc.remove(i, 10);
        return true;
    }

    return false;
}

bool
VodModel::tryGetGame(QString& inoutSrc, Game::GameType* game) {
    int index = inoutSrc.indexOf(QLatin1String("(bw)"), 0, Qt::CaseInsensitive);
    if (index >= 0) {
        inoutSrc.remove(index, 4);
        *game = Game::Broodwar;
        return true;
    }

    return false;
}

//QQmlListProperty<Tournament>
//VodModel::tournaments() const
//{
//    return QQmlListProperty<Tournament>((QObject*)const_cast<VodModel*>(this), NULL, tournamentListCount, tournamentListAt);
//}

//int
//VodModel::tournamentListCount(QQmlListProperty<Tournament>* l) {
//    Q_ASSERT(l);
//    Q_ASSERT(l->object);

//    VodModel* self = static_cast<VodModel*>(l->object);

////    QMutexLocker g(&self->m_Lock);

//    TournamentList* t = self->m_FrontTournaments.loadAcquire();

//    return t->size();
//}

//Tournament*
//VodModel::tournamentListAt(QQmlListProperty<Tournament>* l, int index) {
//    Q_ASSERT(l);
//    Q_ASSERT(l->object);

//    VodModel* self = static_cast<VodModel*>(l->object);

////    QMutexLocker g(&self->m_Lock);

//    TournamentList* t = self->m_FrontTournaments.loadAcquire();

//    if (index < 0 || index >= t->size())
//    {
//        return Q_NULLPTR;
//    }

//    return (*t)[index];
//}

QQmlListProperty<Vod>
VodModel::vods() const
{
    return QQmlListProperty<Vod>((QObject*)const_cast<VodModel*>(this), NULL, vodListCount, vodListAt);
}


int
VodModel::vodListCount(QQmlListProperty<Vod>* l) {
    Q_ASSERT(l);
    Q_ASSERT(l->object);

    VodModel* self = static_cast<VodModel*>(l->object);

    const SoaItem& soaItem = self->m_BackItems[self->m_FronIndex.load()];

    return soaItem.size();
}

Vod*
VodModel::vodListAt(QQmlListProperty<Vod>* l, int index) {
    Q_ASSERT(l);
    Q_ASSERT(l->object);

    VodModel* self = static_cast<VodModel*>(l->object);

    return new Vod(index, self);
}


QHash<int, QByteArray>
VodModel::makeRoleNames() {
    QHash<int, QByteArray> roles;
    const char* Roles[] = {
        "played",
        "side1",
        "side2",
        "url",
        "tournament",
        "season",
        "stage",
        "matchNumber",
        "matchCount",
        "game",
        "icon",
    };

    for (size_t i = 0; i < _countof(Roles); ++i) {
        roles[Qt::UserRole + 1 + i] = Roles[i];
    }

    return roles;
}

QHash<int, QByteArray>
VodModel::roleNames() const {
    return ms_RoleNames;
}

QModelIndex
VodModel::index(int row, int column, const QModelIndex& parent) const {
    if (parent.isValid()) {
        return QModelIndex();
    }

    if (column < 0 || column >= (int)_countof(VodStrings)) {
        return QModelIndex();
    }

    const SoaItem& item = m_BackItems[m_FronIndex.load()];
    if (row < 0 || row >= item.size()) {
        return QModelIndex();
    }

    return createIndex(row, column, Q_NULLPTR);
}

QModelIndex
VodModel::parent(const QModelIndex &child) const {
    Q_UNUSED(child);
    return QModelIndex();
}
