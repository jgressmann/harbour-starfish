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

#include "ScIcons.h"
#include "ScApp.h"

#include <QFile>
#include <QByteArray>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QDebug>

bool
ScIcons::load(QString path) {
    qDebug() << "loading" << path;

    m_IconData.clear();

    QFile file(path);
    if (file.open(QIODevice::ReadOnly)) {
        auto buffer = file.readAll();
        bool ok = false;
        auto decompressed = ScApp::gzipDecompress(buffer, &ok);
        if (ok) {
            buffer = decompressed;
        }

        QJsonParseError parseError;
        auto doc = QJsonDocument::fromJson(buffer, &parseError);
        if (doc.isEmpty()) {
            qWarning("document is empty, error: %s\n%s", qPrintable(parseError.errorString()), qPrintable(buffer));
            return false;
        }

        if (!doc.isObject()) {
            qWarning() << "document has no object root" << buffer;
            return false;
        }

        auto root = doc.object();

        auto version = root[QStringLiteral("version")].toInt();
        if (version <= 0) {
            qWarning() << "invalid document version" << version << buffer;
            return false;
        }

        if (version > 1) {
            qInfo() << "unsupported version" << version << buffer;
            return false;
        }

        auto eventArray = root[QStringLiteral("event")].toArray();

        for (auto i = 0; i < eventArray.size(); ++i) {
            const auto& item = eventArray[i];
            if (!item.isObject()) {
                qWarning() << "event array contains non-objects" << version << buffer;
                return false;
            }

            QJsonObject eventObject = item.toObject();

            QString regex = eventObject[QStringLiteral("regex")].toString();
            bool minimal = eventObject[QStringLiteral("minimal")].toBool();
            bool caseSensitive = eventObject[QStringLiteral("case-sensitive")].toBool();

            QRegExp r(regex, caseSensitive ? Qt::CaseSensitive : Qt::CaseInsensitive);
            if (!r.isValid()) {
                qWarning() << "invalid regex string" << regex << "at index" << i;
                continue;
            }
            r.setMinimal(minimal);

            QString url = eventObject[QStringLiteral("url")].toString();
            if (url.isEmpty()) {
                qWarning() << "empty url string at index" << i;
                continue;
            }

            QString extension = eventObject[QStringLiteral("extension")].toString();
            if (extension.isEmpty()) {
                qWarning() << "empty extension string at index" << i;
                continue;
            }

            ScIconData data;
            data.regex = r;
            data.url =  url;
            data.extension = extension;

            m_IconData << data;
        }

        return true;
    } else {
        qWarning() << "could not open" << path << "for reading";
        return false;
    }
}

bool
ScIcons::getIconForEvent(const QString& event, QString* iconPath) const {
    Q_ASSERT(iconPath);
    foreach (const ScIconData& data, m_IconData) {
        if (data.regex.indexIn(event) >= 0) {
            *iconPath = data.url;
            return true;
        }
    }

    return false;
}

int
ScIcons::size() const {
    return m_IconData.size();
}

QString
ScIcons::url(int index) const {
    return m_IconData[index].url;
}

QString
ScIcons::extension(int index) const {
    return m_IconData[index].extension;
}
