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
#include <QLinkedList>

class QSqlDatabase;
class QNetworkAccessManager;
class QNetworkRequest;
class QNetworkReply;


class VodModel : public QObject {

    Q_OBJECT
    Q_ENUMS(Status)
    Q_ENUMS(Error)
    Q_ENUMS(Game)
    Q_PROPERTY(QDateTime lastUpdated READ lastUpdated NOTIFY lastUpdatedChanged)

    Q_PROPERTY(Status status READ status NOTIFY statusChanged)
    Q_PROPERTY(Error error READ error NOTIFY errorChanged)
    Q_PROPERTY(QString errorString READ errorString NOTIFY errorStringChanged)
    Q_PROPERTY(QString dataDir READ dataDir NOTIFY dataDirChanged)
    Q_PROPERTY(qreal vodFetchingProgress READ vodFetchingProgress NOTIFY vodFetchingProgressChanged)
public:
    enum Game {
        Game_Unknown,
        Game_Broodwar,
        Game_Sc2
    };

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

public:
    ~VodModel();
    explicit VodModel(QObject *parent = Q_NULLPTR);

public: //
    Q_INVOKABLE QString mediaPath(const QString& str) const;
    Q_INVOKABLE QString label(const QString& key, const QVariant& value = QVariant()) const;
    Q_INVOKABLE QString icon(const QString& key, const QVariant& value = QVariant()) const;
    QDateTime lastUpdated() const;
    void setNetworkAccessManager(QNetworkAccessManager* manager);
    Status status() const { return m_Status; }
    Error error() const { return m_Error; }
    QString errorString() const;
    void setDatabase(QSqlDatabase* db);
    QSqlDatabase* database() const { return m_Database; }
    QString dataDir() const;
    qreal vodFetchingProgress() const;
signals:
    void lastUpdatedChanged(QDateTime newValue);
    void statusChanged();
    void errorChanged();
    void errorStringChanged(QString newValue);
    void dataDirChanged(QString newValue);
    void vodFetchingProgressChanged(qreal newValue);
    void vodsAdded();


public slots:
    void poll();

private slots:
    void requestFinished(QNetworkReply* reply);

private:
//    void parseReply(QByteArray& data);
//    void addVods(QByteArray& data);
    static bool tryGetSeason(QString& inoutSrc, int* season);
    static bool tryGetYear(QString& inoutSrc, int* year);
    static bool tryGetGame(QString& inoutSrc, Game* game);
    static bool tryGetDate(QString& inoutSrc, QDateTime* date);
    static bool tryGetIcon(const QString& title, QString* iconPath);
    void createTablesIfNecessary();
    void setStatus(Status value);
    void setError(Error value);
    void updateStatus();
    void parseLevel0(QNetworkReply* reply);
    void parseLevel1(QNetworkReply* reply);
    void parseLevel2(QNetworkReply* reply);
    QNetworkReply* makeRequest(const QUrl& url) const;
    void insertVods();
    void setVodFetchingProgress(qreal value);
    void updateVodFetchingProgress();

    struct TournamentData;
    struct StageData;
    struct MatchData {
        StageData* parent;
        QUrl url;
        QString side1, side2;
        QDate matchDate;
        int matchNumber;
    };

    struct StageData {
        TournamentData* parent;
        QString name;
        QLinkedList<MatchData> matches;
        int index;
    };

    struct TournamentData {
        QString name;
        QString fullName;
        int year;
        int season;
        Game game;
        bool isShow;
        QLinkedList<StageData> stages;
    };

    struct TournamentRequestData {
        TournamentData* tournament;
        QUrl url;
    };

private:
    mutable QMutex m_Lock;
    QTimer m_Timer;
    QNetworkAccessManager* m_Manager;
    QDateTime m_lastUpdated;
    Status m_Status;
    Error m_Error;
    QList<TournamentRequestData> m_TournamentRequestQueue;
    QHash<QNetworkReply*, int> m_PendingRequests;
    QLinkedList<TournamentData> m_Tournaments;
    QHash<QNetworkReply*, TournamentData*> m_RequestStage;
    QHash<QNetworkReply*, StageData*> m_RequestMatch;
    QHash<QNetworkReply*, MatchData*> m_RequestVod;
    QSqlDatabase* m_Database;
    qreal m_VodFetchingProgress;
    int m_TotalUrlsToFetched;
    int m_CurrentUrlsToFetched;
    bool m_AddedVods;

private:
    static const QByteArray ms_UserAgent;

};

