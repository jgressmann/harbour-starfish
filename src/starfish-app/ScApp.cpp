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


#include <zlib.h>
#include <string.h>


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
ScApp::isUrl(const QString& str) {
    QUrl url(str);
    return url.isValid();
}



QByteArray
ScApp::gzipDecompress(const QByteArray& data, bool* ok) {
    //https://stackoverflow.com/questions/2690328/qt-quncompress-gzip-data
    if (ok) *ok = false;

//    if (data.size() <= 4) {
//        qWarning("gUncompress: Input data is truncated");
//        return QByteArray();
//    }

    QByteArray result;
    z_stream strm;
    memset(&strm, 0, sizeof(strm));
    char decompressed[1024];


    /* allocate inflate state */
    strm.avail_in = (uInt)data.size();
    strm.next_in = (Bytef*)data.data();

    int ret = inflateInit2(&strm, 15 +  32); // gzip decoding
    if (ret != Z_OK) {
        return QByteArray();
    }

    // run inflate()
    do {
        strm.avail_out = (uInt)sizeof(decompressed);
        strm.next_out = (Bytef*)(decompressed);

        ret = inflate(&strm, Z_NO_FLUSH);
        Q_ASSERT(ret != Z_STREAM_ERROR);  // state not clobbered

        switch (ret) {
        case Z_NEED_DICT:
            ret = Z_DATA_ERROR;     // and fall through
        case Z_DATA_ERROR:
        case Z_MEM_ERROR:
            (void)inflateEnd(&strm);
            return QByteArray();
        }

        result.append(decompressed, (int)(sizeof(decompressed) - strm.avail_out));
    } while (strm.avail_out == 0);

    // clean up and return
    inflateEnd(&strm);

    if (ok) *ok = true;
    return result;
}


QString ScApp::dataDir() const {
    return QStandardPaths::writableLocation(QStandardPaths::DataLocation);
}

QString ScApp::appDir() const {
    return QStringLiteral(QT_STRINGIFY(SAILFISH_DATADIR));
}

void ScApp::unlink(const QString& filePath){
    QFile::remove(filePath);
}
