#include <QGuiApplication>
#include <QQuickView>
#include <qqml.h>

#include "Sc2LinksDotCom.h"
#include "Sc2CastsDotCom.h"
#include "SqlVodModel.h"
#include "ScVodDatabaseDownloader.h"
#include "ScVodDataManager.h"
#include "ScApp.h"
#include "ScVodman.h"


#include <sailfishapp.h>


static QObject *dataManagerProvider(QQmlEngine *engine, QJSEngine *scriptEngine)
{
    Q_UNUSED(engine)
    Q_UNUSED(scriptEngine)

    return new ScVodDataManager();
}

static QObject *appProvider(QQmlEngine *engine, QJSEngine *scriptEngine)
{
    Q_UNUSED(engine)
    Q_UNUSED(scriptEngine)

    return new ScApp();
}

int
main(int argc, char *argv[]) {

    QScopedPointer<QGuiApplication> app(SailfishApp::application(argc, argv));
    QScopedPointer<QQuickView> view(SailfishApp::createView());


    qmlRegisterType<SqlVodModel>(STARFISH_NAMESPACE, 1, 0, "SqlVodModel");
    qmlRegisterType<ScVodDatabaseDownloader>(STARFISH_NAMESPACE, 1, 0, "VodDatabaseDownloader");
    qmlRegisterType<Sc2LinksDotCom>(STARFISH_NAMESPACE, 1, 0, "Sc2LinksDotComScraper");
    qmlRegisterType<Sc2CastsDotCom>(STARFISH_NAMESPACE, 1, 0, "Sc2CastsDotComScraper");
    qmlRegisterSingletonType<ScApp>(STARFISH_NAMESPACE, 1, 0, "App", appProvider);
    qmlRegisterUncreatableType<ScVodScraper>(STARFISH_NAMESPACE, 1, 0, "VodScraper", "VodScraper");
    qmlRegisterUncreatableType<ScVodman>(STARFISH_NAMESPACE, 1, 0, "Vodman", "Vodman");
    qmlRegisterUncreatableType<VMVodEnums>(STARFISH_NAMESPACE, 1, 0, "VM", QStringLiteral("wrapper around C++ enums"));
    qmlRegisterUncreatableType<ScEnums>(STARFISH_NAMESPACE, 1, 0, "Sc", QStringLiteral("wrapper around C++ enums"));
    qmlRegisterSingletonType<ScVodDataManager>(STARFISH_NAMESPACE, 1, 0, "VodDataManager", dataManagerProvider);


    view->setSource(SailfishApp::pathToMainQml());
    view->requestActivate();
    view->show();
    return app->exec();
}


