/* The MIT License (MIT)
 *
 * Copyright (c) 2018 Jean Gressmann <jean@0x42.de>
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

#include <QGuiApplication>
#include <QQuickView>
#include <qqml.h>

#include "Sc2LinksDotCom.h"
#include "Sc2CastsDotCom.h"
#include "ScSqlVodModel.h"
#include "ScVodDatabaseDownloader.h"
#include "ScVodDataManager.h"
#include "ScApp.h"
#include "ScVodman.h"
#include "ScRecentlyUsedModel.h"


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


    qmlRegisterType<ScSqlVodModel>(STARFISH_NAMESPACE, 1, 0, "SqlVodModel");
    qmlRegisterType<ScVodDatabaseDownloader>(STARFISH_NAMESPACE, 1, 0, "VodDatabaseDownloader");
    qmlRegisterType<Sc2LinksDotCom>(STARFISH_NAMESPACE, 1, 0, "Sc2LinksDotComScraper");
    qmlRegisterType<Sc2CastsDotCom>(STARFISH_NAMESPACE, 1, 0, "Sc2CastsDotComScraper");
    qmlRegisterType<ScRecentlyUsedModel>(STARFISH_NAMESPACE, 1, 0, "RecentlyUsedModel");
//    qmlRegisterUncreatableType<QSqlDatabase>("Qt", 1, 0, "QSqlDatabase", "QSqlDatabase");
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


