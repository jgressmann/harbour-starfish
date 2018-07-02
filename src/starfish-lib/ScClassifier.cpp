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

#include "ScClassifier.h"
#include "Sc.h"

#include <QFile>
#include <QByteArray>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QDebug>

void ScClassifier::Classifier::clear() {
    positive.clear();
    negative.clear();
    remove.clear();
}

void ScClassifier::GameClassifier::clear() {
    event.clear();
    stage.clear();
    match.clear();
}

bool
ScClassifier::load(const QString& path) {
    qDebug() << "loading" << path;

    QFile file(path);
    if (file.open(QIODevice::ReadOnly)) {
        auto buffer = file.readAll();
        bool ok = false;
        auto decompressed = Sc::gzipDecompress(buffer, &ok);
        if (ok) {
            buffer = decompressed;
        }

        return load(buffer);
    } else {
        qWarning() << "could not open" << path << "for reading";
        return false;
    }
}

bool
ScClassifier::load(const QByteArray& buffer) {
    m_Bw.clear();
    m_Sc2.clear();

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

    auto bw = root[QStringLiteral("bw")].toObject();
    auto sc2 = root[QStringLiteral("sc2")].toObject();

    if (!parseGame(m_Bw, bw)) {
        qWarning("failed to parse bw\n%s", qPrintable(buffer));
        return false;
    }

    if (!parseGame(m_Sc2, sc2)) {
        qWarning("failed to parse sc2\n%s", qPrintable(buffer));
        return false;
    }

    return true;
}

bool
ScClassifier::parseGame(GameClassifier& game, const QJsonObject& obj) const {
    if (obj.isEmpty()) {
        qWarning() << "empty game object";
        return false;
    }

    auto event = obj[QStringLiteral("event")].toObject();
    auto stage = obj[QStringLiteral("stage")].toObject();
    auto match = obj[QStringLiteral("match")].toObject();

    if (!parseClassifier(game.event, event)) {
        qWarning() << "failed to parse event";
        return false;
    }

    if (!parseClassifier(game.stage, stage)) {
        qWarning() << "failed to parse stage";
        return false;
    }

    if (!parseClassifier(game.match, match)) {
        qWarning() << "failed to parse match";
        return false;
    }

    return true;
}

bool
ScClassifier::parseClassifier(Classifier& classifier, const QJsonObject& obj) const {
    if (obj.isEmpty()) {
        qWarning() << "empty classifier object";
        return false;
    }

    auto positive = obj[QStringLiteral("+")].toArray();
    auto negative = obj[QStringLiteral("-")].toArray();
    auto remove = obj[QStringLiteral("remove")].toArray();

    if (!parse(classifier.positive, positive)) {
        qWarning() << "failed to parse positive";
        return false;
    }

    if (!parse(classifier.negative, negative)) {
        qWarning() << "failed to parse negative";
        return false;
    }

    if (!parse(classifier.remove, remove)) {
        qWarning() << "failed to parse remove";
        return false;
    }

    return true;
}


bool
ScClassifier::parse(QList<QRegExp>& list, const QJsonArray& arr) const {
    for (auto i = 0, end = arr.size(); i < end; ++i) {
        const auto& item = arr[i];
        if (!item.isObject()) {
            qWarning() << "array contains non-object at index" << i;
            return false;
        }

        QJsonObject obj = item.toObject();

        QString regex = obj[QStringLiteral("regex")].toString();
        bool minimal = obj[QStringLiteral("minimal")].toBool();
        bool caseSensitive = obj[QStringLiteral("case-sensitive")].toBool();

        QRegExp r(regex, caseSensitive ? Qt::CaseSensitive : Qt::CaseInsensitive);
        if (!r.isValid()) {
            qWarning() << "invalid regex string" << regex << "at index" << i;
            continue;
        }
        r.setMinimal(minimal);

        list << r;
    }

    return true;
}

bool
ScClassifier::tryGetGameFromEvent(
        const QString& str,
        ScRecord::Game* game,
        QString* rem) const {

    Q_ASSERT(game);

    QString dummy;
    rem = rem ? rem : &dummy;

    if (classify(m_Bw.event, str, rem)) {
        *game = ScRecord::GameBroodWar;
        return true;
    }

    if (classify(m_Sc2.event, str, rem)) {
        *game = ScRecord::GameSc2;
        return true;
    }

    return false;
}

bool
ScClassifier::tryGetGameFromStage(const QString& str, ScRecord::Game* game, QString* rem) const {
    Q_ASSERT(game);

    QString dummy;
    rem = rem ? rem : &dummy;

    if (classify(m_Bw.stage, str, rem)) {
        *game = ScRecord::GameBroodWar;
        return true;
    }

    if (classify(m_Sc2.stage, str, rem)) {
        *game = ScRecord::GameSc2;
        return true;
    }

    return false;
}
bool
ScClassifier::tryGetGameFromMatch(const QString& str, ScRecord::Game* game, QString* rem) const {
    Q_ASSERT(game);

    QString dummy;
    rem = rem ? rem : &dummy;

    if (classify(m_Bw.match, str, rem)) {
        *game = ScRecord::GameBroodWar;
        return true;
    }

    if (classify(m_Sc2.match, str, rem)) {
        *game = ScRecord::GameSc2;
        return true;
    }

    return false;
}

bool
ScClassifier::classify(const Classifier& classfier, const QString& str, QString* rem) const {
    Q_ASSERT(rem);
    for (const auto& regex : classfier.negative) {
        if (regex.indexIn(str) >= 0) {
            return false;
        }
    }

    auto result = false;
    for (const auto& regex : classfier.positive) {
        if (regex.indexIn(str) >= 0) {
            result = true;
            break;
        }
    }

    if (result) {
        *rem = str;
        for (const auto& regex : classfier.remove) {
            rem->remove(regex);
        }
    }

    return result;
}

void
ScClassifier::cleanEvent(ScRecord::Game game, QString& str) const {
    switch (game) {
    case ScRecord::GameBroodWar:
        for (const auto& regex : m_Bw.event.remove) {
            str.remove(regex);
        }
        break;
    case ScRecord::GameSc2:
        for (const auto& regex : m_Sc2.event.remove) {
            str.remove(regex);
        }
        break;
    default:
        break;
    }
}
