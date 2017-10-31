#pragma once

#include <QAbstractTableModel>
#include <QDateTime>
#include <QMutex>
#include <QTimer>
#include <QList>
#include <QDateTime>
#include <QUrl>
#include <QQmlListProperty>
#include <QAtomicPointer>
#include <QSet>
#include <QVector>

class QSqlDatabase;
class QNetworkAccessManager;
class QNetworkRequest;
class QNetworkReply;

class Game : public QObject {
    Q_OBJECT
public:
    enum GameType {
        Unknown,
        Broodwar,
        Sc2
    };

    Q_ENUMS(GameType)
};

class VodModel;
class Tournament;
class Vod : public QObject {
    Q_OBJECT
//    Q_DISABLE_COPY(Vod)
    Q_PROPERTY(QDateTime played READ played NOTIFY playedChanged)
    Q_PROPERTY(QString side1 READ side1 NOTIFY side1Changed)
    Q_PROPERTY(QString side2 READ side2 NOTIFY side2Changed)
    Q_PROPERTY(QUrl url READ url NOTIFY urlChanged)
    Q_PROPERTY(QString tournament READ tournament NOTIFY tournamentChanged)
    Q_PROPERTY(int season READ season NOTIFY seasonChanged)
    Q_PROPERTY(QString stage READ stage NOTIFY stageChanged)
    Q_PROPERTY(int matchNumber READ matchNumber NOTIFY matchNumberChanged)
    Q_PROPERTY(int matchCount READ matchCount NOTIFY matchCountChanged)
    Q_PROPERTY(Game::GameType game READ game NOTIFY gameChanged)
    Q_PROPERTY(QString icon READ icon NOTIFY iconChanged)
//    Q_PROPERTY(VodModel* model READ model)
public:
    ~Vod();
    explicit Vod(int index, VodModel* parent);
public:
    QDateTime played() const;
    QString side1() const;
    QString side2() const;
    QUrl url() const;
    QString tournament() const;
    int season() const;
    QString stage() const;
    int matchNumber() const;
    int matchCount() const;
    Game::GameType game() const;
    QString icon() const;
    const VodModel* model() const;
signals:
    void playedChanged();
    void side1Changed();
    void side2Changed();
    void urlChanged();
    void tournamentChanged();
    void seasonChanged();
    void stageChanged();
    void matchNumberChanged();
    void matchCountChanged();
    void gameChanged();
    void iconChanged();

private:
    int m_Index;
};

//class Tournament : public

class VodModel //: public QAbstractTableModel {
        //: public QObject {
        //: public QAbstractListModel  {
        : public QAbstractItemModel {
    Q_OBJECT
    Q_ENUMS(Status)
    Q_ENUMS(Error)
    Q_PROPERTY(QDateTime lastUpdated READ lastUpdated NOTIFY lastUpdatedChanged)

//    Q_PROPERTY(QQmlListProperty<Tournament> tournaments READ tournaments NOTIFY tournamentsChanged)
//    Q_PROPERTY(QQmlListProperty<Vod> vods READ vods NOTIFY vodsChanged)
    Q_PROPERTY(Status status READ status NOTIFY statusChanged)
    Q_PROPERTY(Error error READ error NOTIFY errorChanged)
    Q_PROPERTY(QString errorString READ errorString NOTIFY errorStringChanged)
    Q_PROPERTY(QString dataDir READ dataDir NOTIFY dataDirChanged)


public:
    enum Status {
        Status_Uninitialized,
        Status_Ready,
        Status_VodFetchingInProgress,
        Status_VodFetchingComplete,
        Status_Error,
    };

    enum Error {
        Error_None,
        Error_CouldntCreateSqlTables,
        Error_CouldntStartTransaction,
        Error_CouldntEndTransaction,
        Error_SqlTableManipError,
    };

    enum Key {
        VodDate = 0,
        VodSide1, // Taeja, Poland
        VodSide2, // MVP, France
        VodUrl, //
        VodTournament, // GSL, Homestory Cup
        VodSeason,  // Season 1, XII
        VodStage, // RO16, Day 1, etc
        VodMatchNumber, // Match in best of X
        VodMatchCount, // Best of X
        VodGame, // Broodwar, Sc2
        VodIcon,
    };



public:
    ~VodModel();
    explicit VodModel(QObject *parent = Q_NULLPTR);

public: // QAbstractTableModel
    //When subclassing QAbstractTableModel, you must implement rowCount(), columnCount(), and data(). Default implementations of the index() and parent() functions are provided by QAbstractTableModel. Well behaved models will also implement headerData().
    int rowCount(const QModelIndex &parent) const;
    int columnCount(const QModelIndex &parent) const;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const;
    QVariant headerData(int section, Qt::Orientation orientation, int role) const;
    QHash<int, QByteArray> roleNames() const;
    QModelIndex index(int row, int column, const QModelIndex& parent = QModelIndex()) const;
    QModelIndex parent(const QModelIndex &child) const;

public: //
    Q_INVOKABLE QString mediaPath(const QString& str) const;
    Q_INVOKABLE QString label(const QString& key, const QVariant& value) const;
    Q_INVOKABLE QString icon(const QString& key, const QVariant& value) const;
    QDateTime lastUpdated() const;
    void setNetworkAccessManager(QNetworkAccessManager* manager);
//    QQmlListProperty<Tournament> tournaments() const;
//    QAbstractTableModel* model() const { return const_cast<VodModel*>(this); }
    QQmlListProperty<Vod> vods() const;
    Status status() const { return m_Status; }
    Error error() const { return m_Error; }
    QString errorString() const;
    void setDatabase(QSqlDatabase* db);
    QSqlDatabase* database() const { return m_Database; }
    QString dataDir() const;

public: // Vod
    QDateTime played(int index) const;
    QString side1(int index) const;
    QString side2(int index) const;
    QUrl url(int index) const;
    QString tournament(int index) const;
    int season(int index) const;
    QString stage(int index) const;
    int matchNumber(int index) const;
    int matchCount(int index) const;
    Game::GameType game(int index) const;
    QString icon(int index) const;

signals:
    void lastUpdatedChanged(QDateTime newValue);
//    void tournamentsChanged();
//    void modelChanged();
    void vodsChanged();
    void statusChanged();
    void errorChanged();
    void errorStringChanged(QString newValue);
    void dataDirChanged(QString newValue);

public slots:
    void poll();

private slots:
    void requestFinished(QNetworkReply* reply);

private:
    void parseReply(QByteArray& data);
    static int tournamentListCount(QQmlListProperty<Tournament>* l);
    static Tournament* tournamentListAt(QQmlListProperty<Tournament>* l, int index);
    static int vodListCount(QQmlListProperty<Vod>* l);
    static Vod* vodListAt(QQmlListProperty<Vod>* l, int index);
    void addVods(QByteArray& data);
    static bool tryGetSeason(QString& inoutSrc, int* season);
    static bool tryGetYear(QString& inoutSrc, int* year);
    static bool tryGetGame(QString& inoutSrc, Game::GameType* game);
    static bool tryGetDate(QString& inoutSrc, QDateTime* date);
    static bool tryGetIcon(const QString& title, QString* iconPath);
    static QHash<int, QByteArray>  makeRoleNames();
    void createTablesIfNecessary();
    void setStatus(Status value);
    void setError(Error value);
    void updateStatus();


    struct Item {
        QDateTime played;
        QString side1;
        QString side2;
        QUrl url;
        QString tournament;
        int season;
        QString stage;
        int matchNumber;
        int matchCount;
        Game::GameType game;
        int year;
        QString icon;
    };

    struct SoaItem {
        QVector<QDateTime> played;
        QVector<QString> side1;
        QVector<QString> side2;
        QVector<QUrl> url;
        QVector<QString> tournament;
        QVector<int> season;
        QVector<QString> stage;
        QVector<int> matchNumber;
        QVector<int> matchCount;
        QVector<Game::GameType> game;
        QVector<int> year;
        QVector<QString> icon;

        int size() const;
        void clear();
    };

    struct TournamentRequests {
        QString name;
        int year;
        QUrl url;
    };
    typedef QList<Tournament*> TournamentList;
    typedef QAtomicPointer<TournamentList> AtomicListPtr;
    typedef QList<Item> ItemList;
    typedef QAtomicPointer<ItemList> AtomicItemListPtr;
    typedef QList<Vod> VodList;
    typedef QAtomicPointer<VodList> AtomicVodListPtr;

private:
    mutable QMutex m_Lock;
    QTimer m_Timer;
    QNetworkAccessManager* m_Manager;
    QDateTime m_lastUpdated;
    Status m_Status;
    Error m_Error;
//    ItemList m_BackItemLists[2];
//    AtomicItemListPtr m_FrontItemList;
//        ItemList m_BackVodLists[2];
//        AtomicItemListPtr m_FrontVodList;

    SoaItem m_BackItems[2];
    QAtomicInt m_FronIndex;

//    QList<Vod*> m_Vods;
//    QList<Item> m_Items;
//    QHash<QString, TournamentRequests> m_PendingTournamentRequests;
    QSet<QNetworkReply*> m_PendingRequests;
//    m_RequestToTournamentMap
//    QHash<QNetworkReply*, int> m_RequestToTournamentMap;
//    int m_PendingRequests;
    QSqlDatabase* m_Database;
private:
    static const QByteArray ms_UserAgent;
    static const QHash<int, QByteArray> ms_RoleNames;
};

