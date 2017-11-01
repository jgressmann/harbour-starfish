#ifdef QT_QML_DEBUG
#include <QtQuick>
#endif


#include <QDir>
#include <QGuiApplication>
#include <QQmlEngine>
#include <QQuickView>
#include <QQmlContext>
#include <QQmlApplicationEngine>
#include <QNetworkAccessManager>
#include <QSqlDatabase>
#include <QStandardPaths>
#include <QSqlError>
//#include <QTcpSocket>
//#ifndef SAILFISH
//#include <QCommandLineParser>
//#include <QQmlFileSelector>
//#include "Style.h"
//#endif
#include "Sc2LinksDotCom.h"
#include "DistinctModel.h"
#include "SqlVodModel.h"
#include <sailfishapp.h>

#define NAMESPACE "org.duckdns.jgressmann"

static QNetworkAccessManager* s_NetworkAccessManager;
static QSqlDatabase* s_Database;

template<typename T>
static
QObject*
make(QQmlEngine *, QJSEngine *) {
    return new T;
}

template<typename T>
static
QObject*
makeModel(QQmlEngine *, QJSEngine *) {
    VodModel* t = new T();
    t->setNetworkAccessManager(s_NetworkAccessManager);
    t->setDatabase(s_Database);
    return t;
}

static
int
RunMobile(QGuiApplication& app)
{
    QScopedPointer<QQuickView> v(SailfishApp::createView());
    QQuickView& view = *v;

    view.engine()->addImportPath("qrc:/qml");
    view.setSource(SailfishApp::pathTo(QStringLiteral("qml/harbour-starfish.qml")));

    view.show();

    return app.exec();
}




SAILFISHAPP_EXPORT
int
main(int argc, char *argv[]) {
    int error = 0;
    QScopedPointer<QGuiApplication> appPtr(SailfishApp::application(argc, argv));
    QGuiApplication& app = *appPtr;

#ifdef SAILFISH_DATADIR
    //QString databaseFilePath(QStringLiteral(QT_STRINGIFY(SAILFISH_DATADIR) "/db.sqlite"));
    //QString databaseFilePath(QStandardPaths::writableLocation((QStandardPaths::AppLocalDataLocation)) + QStringLiteral("/db.sqlite"));
#else
    //QString databaseFilePath(QStandardPaths::writableLocation((QStandardPaths::AppLocalDataLocation)) + QStringLiteral("/db.sqlite"));
#endif
    QDir dir(QStandardPaths::writableLocation((QStandardPaths::AppLocalDataLocation)));
    if (!dir.exists()) {
        dir.mkpath(dir.path());
    }
    QString databaseFilePath = dir.absoluteFilePath("db.sqlite");
    QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE");
    db.setDatabaseName(databaseFilePath);
    if (db.open()) {
        QNetworkAccessManager m;
        qmlRegisterType<SqlVodModel>(NAMESPACE, 1, 0, "SqlVodModel");
        qmlRegisterSingletonType<VodModel>(NAMESPACE, 1, 0, "Sc2LinksDotCom", (QObject* (*)(QQmlEngine*, QJSEngine*))makeModel<VodModel>);

        s_NetworkAccessManager = &m;
        s_Database = &db;
        error = RunMobile(app);
        s_Database = Q_NULLPTR;
        s_NetworkAccessManager = Q_NULLPTR;

        db.close();
    } else {
        auto sqlError = db.lastError();
        qCritical() << "Could not open database" << sqlError;
        error = -1;
    }

    return error;
}


