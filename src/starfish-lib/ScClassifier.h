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

#pragma once

#include "ScRecord.h"
#include <QRegExp>
#include <QJsonArray>
#include <QJsonObject>

class ScClassifier
{
public:
    bool load(const QString& path);
    bool load(const QByteArray& buffer);

    bool tryGetGameFromEvent(const QString& str, ScRecord::Game* game, QString* rem = nullptr) const;
    bool tryGetGameFromStage(const QString& str, ScRecord::Game* game, QString* rem = nullptr) const;
    bool tryGetGameFromMatch(const QString& str, ScRecord::Game* game, QString* rem = nullptr) const;
    void cleanEvent(ScRecord::Game game, QString& str) const;

private:
    struct Classifier {
        QList<QRegExp> positive, negative, remove;
        void clear();
    };

    struct GameClassifier {
        Classifier event;
        Classifier stage;
        Classifier match;

        void clear();
    };

private:
    bool parseGame(GameClassifier& game, const QJsonObject& obj) const;
    bool parseClassifier(Classifier& classifier, const QJsonObject& obj) const;
    bool parse(QList<QRegExp>& list, const QJsonArray& arr) const;
    bool classify(const Classifier& classfier, const QString& str, QString* rem) const;

private:
    GameClassifier m_Bw;
    GameClassifier m_Sc2;
};
