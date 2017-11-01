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
signals:
    void lastUpdatedChanged(QDateTime newValue);
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
    void addVods(QByteArray& data);
    static bool tryGetSeason(QString& inoutSrc, int* season);
    static bool tryGetYear(QString& inoutSrc, int* year);
    static bool tryGetGame(QString& inoutSrc, Game* game);
    static bool tryGetDate(QString& inoutSrc, QDateTime* date);
    static bool tryGetIcon(const QString& title, QString* iconPath);
    void createTablesIfNecessary();
    void setStatus(Status value);
    void setError(Error value);
    void updateStatus();


private:
    mutable QMutex m_Lock;
    QTimer m_Timer;
    QNetworkAccessManager* m_Manager;
    QDateTime m_lastUpdated;
    Status m_Status;
    Error m_Error;
    QSet<QNetworkReply*> m_PendingRequests;
    QSqlDatabase* m_Database;

private:
    static const QByteArray ms_UserAgent;

};

