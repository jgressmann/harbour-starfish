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

#include "ScApp.h"

#include <QNetworkConfiguration>
#include <QNetworkConfigurationManager>
#include <QUrl>
#include <QStandardPaths>
#include <QFile>
#include <QDebug>



ScApp::ScApp(QObject* parent)
    : QObject(parent)
{
    m_NetworkConfigurationManager = new QNetworkConfigurationManager(this);
}

QString
ScApp::version() const {
    return QStringLiteral("%1.%2.%3").arg(QString::number(STARFISH_VERSION_MAJOR), QString::number(STARFISH_VERSION_MINOR), QString::number(STARFISH_VERSION_PATCH));
}

QString
ScApp::displayName() const {
    return QStringLiteral("Starfish");
}

bool
ScApp::isOnBroadband() const {
    auto configs = m_NetworkConfigurationManager->allConfigurations(QNetworkConfiguration::Active);
    foreach (const auto& config, configs) {
        if (config.isValid()) {
            switch (config.bearerTypeFamily()) {
            case QNetworkConfiguration::BearerEthernet:
            case QNetworkConfiguration::BearerWLAN:
                return true;
            default:
                break;
            }
        }
    }

    return false;
}

bool
ScApp::isOnMobile() const {
    auto configs = m_NetworkConfigurationManager->allConfigurations(QNetworkConfiguration::Active);
    foreach (const auto& config, configs) {
        if (config.isValid()) {
            switch (config.bearerTypeFamily()) {
            case QNetworkConfiguration::Bearer2G:
            case QNetworkConfiguration::Bearer3G:
            case QNetworkConfiguration::Bearer4G:
                return true;
            default:
                break;
            }
        }
    }

    return false;
}

bool
ScApp::isUrl(const QString& str) const {
    QUrl url(str);
    return url.isValid();
}

QString ScApp::dataDir() const {
    return QStandardPaths::writableLocation(QStandardPaths::DataLocation);
}

QString ScApp::appDir() const {
    return QStringLiteral(QT_STRINGIFY(SAILFISH_DATADIR));
}

QString ScApp::logDir() const {
    return staticLogDir();
}

QString ScApp::staticLogDir() {
    return QStandardPaths::writableLocation(QStandardPaths::DataLocation) + QStringLiteral("/logs");
}

bool ScApp::unlink(const QString& filePath) const {
    return QFile::remove(filePath);
}

bool ScApp::copy(const QString& srcFilePath, const QString& dstFilePath) const {
    return QFile::copy(srcFilePath, dstFilePath);
}

bool ScApp::move(const QString& srcFilePath, const QString& dstFilePath) const {
    return QFile::rename(srcFilePath, dstFilePath);
}
