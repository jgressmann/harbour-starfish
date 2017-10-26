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
//#include <QTcpSocket>
//#ifndef SAILFISH
//#include <QCommandLineParser>
//#include <QQmlFileSelector>
//#include "Style.h"
//#endif
#include "Sc2LinksDotCom.h"
#include "DistinctModel.h"

#define NAMESPACE "org.duckdns.jgressmann"

static QNetworkAccessManager* s_NetworkAccessManager;

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
    return t;
}



#ifdef SAILFISH
#if defined(LIBSAILFISHAPP_LIBRARY)
#  define SAILFISHAPP_EXPORT Q_DECL_EXPORT
#else
#  define SAILFISHAPP_EXPORT Q_DECL_IMPORT
#endif
namespace SailfishApp {
    // Simple interface: Get boosted application and view
    SAILFISHAPP_EXPORT QGuiApplication *application(int &argc, char **argv);
    SAILFISHAPP_EXPORT QQuickView *createView();

    // Get fully-qualified path to a file in the data directory
    SAILFISHAPP_EXPORT QUrl pathTo(const QString &filename);

    // Get fully-qualified path to the "qml/<appname>.qml"  file
    SAILFISHAPP_EXPORT QUrl pathToMainQml();

    // Very simple interface: Uses "qml/<appname>.qml" as QML entry point
    SAILFISHAPP_EXPORT int main(int &argc, char **argv);
}
static
int
RunMobile(QGuiApplication& app)
{
    QScopedPointer<QQuickView> v(SailfishApp::createView());
    QQuickView& view = *v;

//    view.engine()->rootContext()->setContextProperty(QStringLiteral("cozy"), &cozy);
//    view.connect(view.engine(), SIGNAL(quit()), &app, SLOT(quit()));

    view.engine()->addImportPath("qrc:/qml");
    view.setSource(SailfishApp::pathTo(QStringLiteral("qml/harbour-starfish.qml")));

    view.show();

    return app.exec();
}
#else // !#ifdef SAILFISH

#define SAILFISHAPP_EXPORT

static
int
RunMobile(QGuiApplication& app, Cozy& cozy, bool fullscreen)
{
    QQuickView view;
    view.setResizeMode(QQuickView::SizeRootObjectToView);
    if (qgetenv("QT_QUICK_CORE_PROFILE").toInt())
    {
        QSurfaceFormat f = view.format();
        f.setProfile(QSurfaceFormat::CoreProfile);
        f.setVersion(4, 4);
        view.setFormat(f);
    }
    view.engine()->rootContext()->setContextProperty(QStringLiteral("cozy"), &cozy);
    view.connect(view.engine(), SIGNAL(quit()), &app, SLOT(quit()));
    view.engine()->addImportPath("qrc:/qml");

    new QQmlFileSelector(view.engine(), &view);
    view.setSource(QUrl("qrc:/main-mobile.qml"));

    if (fullscreen)
    {
        view.showFullScreen();
    }
    else
    {
        view.show();
    }

    return app.exec();
}
#endif // !#ifdef SAILFISH


//static
//int
//RunDesktop(QGuiApplication& app, Cozy& cozy)
//{
//    QQmlApplicationEngine engine;
//    engine.rootContext()->setContextProperty(QStringLiteral("cozy"), &cozy);
//    engine.load(QUrl(QStringLiteral("qrc:/main-desktop.qml")));

//    return app.exec();
//}

SAILFISHAPP_EXPORT
int
main(int argc, char *argv[])
{
    bool detectStyle = true;
    bool desktopStyle = true;
    bool fullscreen = false;
    bool useMockSocket = false;
#ifdef SAILFISH
    QScopedPointer<QGuiApplication> appPtr(SailfishApp::application(argc, argv));
    QGuiApplication& app = *appPtr;


#else
    QGuiApplication app(argc, argv);
    app.setOrganizationName("Jean Gressmann");
    app.setOrganizationDomain("jgressmann.duckdns.org");
    app.setApplicationName("Cozy Climate Control");
    app.setApplicationVersion("1.0");


#if (!defined(WINAPI_FAMILY) || WINAPI_FAMILY != WINAPI_FAMILY_PHONE_APP)
    QCommandLineParser parser;
    parser.setApplicationDescription("Cozy climate control ui");
    QCommandLineOption helpOption = parser.addHelpOption();
    QCommandLineOption versionOption = parser.addVersionOption();
    QCommandLineOption mockSocketOption(QStringList() << "mock-socket", QCoreApplication::translate("main", "Use mock socket."));
    parser.addOption(mockSocketOption);
    QCommandLineOption styleOption(QStringList() << "s" << "style",
               QCoreApplication::translate("main", "Select UI <style>."),
               QCoreApplication::translate("main", "style"));
    parser.addOption(styleOption);
    QCommandLineOption noFullscreenOption(QStringList() << "no-fullscreen", QCoreApplication::translate("main", "Don't run in full screen mode."));
    parser.addOption(noFullscreenOption);


    // Process the actual command line arguments given by the user
    parser.process(app);

    if (parser.isSet(helpOption))
    {
        parser.showHelp();
    }

    useMockSocket = parser.isSet(mockSocketOption);
    detectStyle = !parser.isSet(styleOption);
    desktopStyle = !detectStyle && (parser.value(styleOption) == QStringLiteral("desktop") || parser.value(styleOption) == QStringLiteral("window"));
    fullscreen = !parser.isSet(noFullscreenOption);
#else
    useMockSocket = false;
    detectStyle = false;
    desktopStyle = false;
    fullscreen = true;
#endif
#endif // #ifdef SAILFISH


//    MockSocket mockSocket;
//    QTcpSocket tcpSocket;
//    Cozy cozy;

//    if (useMockSocket)
//    {
//        cozy.setSocket(&mockSocket);
//    }
//    else
//    {
//        cozy.setSocket(&tcpSocket);
//    }

    QNetworkAccessManager networkAccessManager;
    s_NetworkAccessManager = &networkAccessManager;

    qmlRegisterType<DistinctModel>(NAMESPACE, 1, 0, "DistinctModel");
    qmlRegisterUncreatableType<Vod>(NAMESPACE, 1, 0, "Vod", "shim");
    qmlRegisterSingletonType<VodModel>(NAMESPACE, 1, 0, "Sc2LinksDotCom", (QObject* (*)(QQmlEngine*, QJSEngine*))makeModel<VodModel>);


//    if (detectStyle)
//    {
//        const QString platformName = QGuiApplication::platformName();

//        if (platformName == QStringLiteral("qnx") ||
//            platformName == QStringLiteral("eglfs") ||
//            platformName == QStringLiteral("android"))
//        {
//            desktopStyle = false;
//        }
//        else
//        {
//            desktopStyle = true;
//        }
//    }

//    if (desktopStyle)
//    {
//        return RunDesktop(app, cozy);
//    }
#ifdef SAILFISH
    return RunMobile(app);
#else
    return RunMobile(app, cozy, fullscreen);
#endif
}


