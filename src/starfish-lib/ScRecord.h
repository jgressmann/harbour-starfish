/* The MIT License (MIT)
 *
 * Copyright (c) 2018, 2019 Jean Gressmann <jean@0x42.de>
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

#include <QObject>
#include <QDate>
#include <QDomNode>
#include <QDebug>

bool scTryGetDate(QString& inoutSrc, QDate* date);

class ScClassifier;
class ScRecord
{
public:
    enum {
        ValidGame = 0x0001,
        ValidYear = 0x0002,
        ValidSeason = 0x0004,
        ValidEventName = 0x0008,
        ValidStageName = 0x0010,
        ValidMatchDate = 0x0020,
        ValidSides = 0x0040,
        ValidRaces = 0x0080,
        ValidUrl = 0x0100,
        ValidMatchName = 0x0200,
        ValidEventFullName = 0x0400,
        ValidMatchNumber = 0x0800,
        ValidStageRank = 0x1000,
    };

    enum Game {
        GameBroodWar,
        GameSc2,
        GameOverwatch,
    };

    enum Race {
        RaceUnknown,
        RaceProtoss,
        RaceTerran,
        RaceZerg,
        RaceRandom,
    };

    QString eventFullName;
    QString eventName;
    QString stage;
    QString side1Name;
    QString side2Name;
    QString url;
    QString matchName;
    QDate matchDate;
    qint32 valid;
    qint32 year;
    qint8 season;
    qint8 matchNumber;
    qint8 stageRank;
    qint8 game : 4;
    qint8 side1Race : 4;
    qint8 side2Race : 4;

    ~ScRecord() = default;
    ScRecord();
    ScRecord(const ScRecord&) = default;
    ScRecord& operator=(const ScRecord&) = default;

    void autoComplete(const ScClassifier& classifier);
    inline bool isValid(int flags) const { return (valid & flags) == flags; }


    QDomNode toXml(QDomDocument& doc) const;
    static ScRecord fromXml(const QDomNode& node);
};

Q_DECLARE_METATYPE(ScRecord)

QDebug operator<<(QDebug debug, const ScRecord& value);


