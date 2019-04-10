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

#include "ScRecord.h"
#include "ScClassifier.h"

#include <QRegExp>
#include <QDebugStateSaver>




#include <vector>
#include <utility>
#include <string>

#ifndef _countof
#   define _countof(x) (sizeof(x)/sizeof((x)[0]))
#endif

namespace
{

//https://stackoverflow.com/questions/12323297/converting-a-string-to-roman-numerals-in-c
typedef std::pair<int, std::string> valueMapping;
std::vector<valueMapping> importantNumbers = {
    {1000, "M"}, {900, "CM"}, {500, "D"}, {400, "CD"},
    {100,  "C"}, { 90, "XC"}, { 50, "L"}, { 40, "XL"},
    {10,   "X"}, {  9, "IX"}, {  5, "V"}, {  4, "IV"},
    {1,    "I"},
};

QString romanNumeralFor(int n, int markCount = 0) {
    std::string result;
    bool needMark = false;
    std::string marks(markCount, '\'');
    for (auto mapping : importantNumbers) {
        int value = mapping.first;
        std::string &digits = mapping.second;
        while (n >= value) {
            result += digits;
            n -= value;
            needMark = true;
        }
        if ((value == 1000 || value == 100 || value == 10 || value == 1) && needMark) {
            result += marks;
            needMark = false;
        }
    }
    return QString::fromStdString(result);
}

const QString EnglishWordSeasons[] = {
    QStringLiteral("season one"),
    QStringLiteral("season two"),
    QStringLiteral("season three"),
    QStringLiteral("season four"),
    QStringLiteral("season five"),
    QStringLiteral("season six"),
    QStringLiteral("season seven"),
    QStringLiteral("season eight"),
    QStringLiteral("season nine"),
    QStringLiteral("season ten"),
};



const QRegExp eventNameReplaceWithSpaces(QStringLiteral("\\s+-\\s+"));
const QRegExp eventNameCrudRegex(QStringLiteral("[\\[\\]/,:]|\\(\\)"));
const QRegExp dateRegex(QStringLiteral("\\d{4}-\\d{2}-\\d{2}"));
const QRegExp yearRegex(QStringLiteral("^\\d{4} | \\d{4}"));
const QRegExp yearRangeRegex(QStringLiteral("\\(?(\\d{4})-\\d{4}\\)?"));

// September 10th 2011
const QRegExp englishDate(QStringLiteral("(?:january|febuary|march|april|may|june|july|august|september|october|november|december)\\s+([0-9]{1,2}(?:st|nd|rd|th)\\s+\\d{4})"), Qt::CaseInsensitive);
const QRegExp sidesRegex(QStringLiteral("(.*)\\s+(?:-|vs)\\s+(.*)"), Qt::CaseInsensitive);
const QRegExp matchNumberRegex(QStringLiteral("^\\s*(?:match|episode)\\s+(\\d+)\\s*$"), Qt::CaseInsensitive);

int InitializeStatics() {
    Q_ASSERT(eventNameReplaceWithSpaces.isValid());
    Q_ASSERT(eventNameCrudRegex.isValid());
    Q_ASSERT(dateRegex.isValid());
    Q_ASSERT(yearRegex.isValid());
    Q_ASSERT(yearRangeRegex.isValid());
    Q_ASSERT(englishDate.isValid());
    Q_ASSERT(sidesRegex.isValid());
    Q_ASSERT(matchNumberRegex.isValid());


    return 1;
}

int s_StaticsInitialized = InitializeStatics();

bool
tryGetSeason(QString& inoutSrc, int* season) {

    inoutSrc = inoutSrc.trimmed();

    // season one, season two, season three, ...
    for (size_t i = 0; i < _countof(EnglishWordSeasons); ++i) {
        int index = inoutSrc.indexOf(EnglishWordSeasons[i], 0, Qt::CaseInsensitive);
        if (index >= 0) {
            *season = i + 1;
            inoutSrc.remove(index, EnglishWordSeasons[i].size());
            return true;
        }
    }

    for (int i = 50; i >= 0; --i) {

        QString romanNumeral = romanNumeralFor(i + 1);



        // s1, s2, s3, ...
        QString shortSeasonString = QStringLiteral("s%1").arg(i+1);
        auto index = inoutSrc.indexOf(shortSeasonString, 0, Qt::CaseInsensitive);
        if (index >= 0) {
            *season = i + 1;;
            inoutSrc.remove(index, shortSeasonString.size());
            return true;
        }

        // season 1, season 2, ...
        QString longSeasonString = QStringLiteral("season %1").arg(i+1);
        index = inoutSrc.indexOf(longSeasonString, 0, Qt::CaseInsensitive);
        if (index >= 0) {
            *season = i + 1;;
            inoutSrc.remove(index, longSeasonString.size());
            return true;
        }

        // I, II, III, IV, ...
        index = inoutSrc.indexOf(romanNumeral, 0, Qt::CaseInsensitive);
        auto hasRomanNumeral = false;
        if (index >= 0) {
            // at end? then there must be whitespace in front of the numeral
            if (index + romanNumeral.size() == inoutSrc.size() &&
                    index > 0 &&
                    inoutSrc[index-1].isSpace()) {
                hasRomanNumeral = true;
//            // at begin? then ws must be following
//            } else if (index == 0 &&
//                       index + romanNumeral.size() < inoutSrc.size() &&
//                       inoutSrc[romanNumeral.size()].isSpace()) {
//                hasRomanNumeral = true;
            // surrounded by ws?
            } else if (index > 0 &&
                       index + romanNumeral.size() < inoutSrc.size() &&
                       inoutSrc[index + romanNumeral.size()].isSpace() &&
                       inoutSrc[index-1].isSpace()){
                hasRomanNumeral = true;
            }
        }

        if (hasRomanNumeral) {
            QString romanSeasonString = QStringLiteral("season ") + romanNumeral;
            auto seasonIndex = inoutSrc.indexOf(romanSeasonString, 0, Qt::CaseInsensitive);
            if (seasonIndex >= 0) {
                inoutSrc.remove(seasonIndex, romanSeasonString.size());
            } else {
                inoutSrc.remove(index, romanNumeral.size());
            }
            *season = i + 1;
            return true;
        }


        // arabic number at end
        QString arabicNumberAtEnd = QStringLiteral(" %1").arg(i+1);
        if (inoutSrc.endsWith(arabicNumberAtEnd)) {
            *season = i + 1;
            inoutSrc.resize(inoutSrc.size() - arabicNumberAtEnd.size());
            return true;
        }
    }

    return false;
}



bool
tryGetYear(QString& inoutSrc, int* year) {
    int index = yearRangeRegex.indexIn(inoutSrc);
    if (index >= 0) {
        *year = yearRangeRegex.cap(1).toInt();
        inoutSrc.remove(yearRangeRegex.cap(0));
        return true;
    }

    index = yearRegex.indexIn(inoutSrc);
    if (index >= 0) {
        *year = yearRegex.cap(0).toInt();
        inoutSrc.remove(yearRegex.cap(0));
        return true;
    }

    return false;
}

bool tryGetSides(const QString& str, QString* side1, QString* side2) {
    int index = sidesRegex.indexIn(str);
    if (index >= 0) {
        *side1 = sidesRegex.cap(1);
        *side2 = sidesRegex.cap(2);
        return true;
    }
    return false;
}

bool tryGetMatchNumber(const QString& str, qint8* matchNumber) {
    int index = matchNumberRegex.indexIn(str);
    if (index >= 0) {
        *matchNumber = static_cast<qint8>(matchNumberRegex.cap(1).toInt());
        return true;
    }
    return false;
}

void removeCrud(QString& inoutSrc) {
    inoutSrc.remove(yearRegex);
    inoutSrc.replace(eventNameReplaceWithSpaces, QStringLiteral(" "));
    inoutSrc.remove(eventNameCrudRegex);
    inoutSrc = inoutSrc.split(' ', QString::SkipEmptyParts).join(' ');
}

} // anon

bool
scTryGetDate(QString& inoutSrc, QDate* date)
{
    (void)s_StaticsInitialized;

    int i = dateRegex.indexIn(inoutSrc);
    if (i >= 0) {
        QStringRef subString(&inoutSrc, i, 10);
        *date = QDate::fromString(subString.toString(), QStringLiteral("yyyy-MM-dd"));
        inoutSrc.remove(i, 10);
        return true;
    }

    i = englishDate.indexIn(inoutSrc);
    if (i >= 0) {
        auto dateStr = englishDate.cap(0).toLower();
        inoutSrc.remove(i, dateStr.size());
        auto parts = dateStr.split(' ');
        int month = 0;
        int day = 0;
        int year = parts[2].toInt();

        if (parts[0] == QStringLiteral("january")) {
            month = 1;
        } else if (parts[0] == QStringLiteral("febuary")) {
            month = 2;
        } else if (parts[0] == QStringLiteral("march")) {
            month = 3;
        } else if (parts[0] == QStringLiteral("april")) {
            month = 4;
        } else if (parts[0] == QStringLiteral("may")) {
            month = 5;
        } else if (parts[0] == QStringLiteral("june")) {
            month = 6;
        } else if (parts[0] == QStringLiteral("july")) {
            month = 7;
        } else if (parts[0] == QStringLiteral("august")) {
            month = 8;
        } else if (parts[0] == QStringLiteral("september")) {
            month = 9;
        } else if (parts[0] == QStringLiteral("october")) {
            month = 10;
        } else if (parts[0] == QStringLiteral("november")) {
            month = 11;
        } else if (parts[0] == QStringLiteral("december")) {
            month = 12;
        }

        for (auto i = 0; i < parts[1].size(); ++i) {
            if (parts[1][i] < '0' || parts[1][i] > '9') {
                day = parts[1].left(i).toInt();
                break;
            }
        }

        *date = QDate(year, month, day);
        return true;
    }

    return false;
}

ScRecord::ScRecord()
{
    valid = 0;
    year = -1;
    game = -1;
    season = -1;
    side1Race = RaceUnknown;
    side2Race = RaceUnknown;
    matchNumber = -1;
    stageRank = -1;
}


void
ScRecord::autoComplete(const ScClassifier& classifier) {
    QString str;
    for (bool change = true; change; ) {
        change = false;

        if (isValid(ValidEventFullName)) {
            if (str.isEmpty()) {
                str = eventFullName;
            }

            // to remove parts of str
            QDate date;
            auto result = scTryGetDate(str, &date);
            if (!isValid(ScRecord::ValidYear) && result) {
                year = date.year();
                valid |= ScRecord::ValidYear;
                change = true;
            }

            int y;
            result = tryGetYear(str, &y);
            if (!isValid(ScRecord::ValidYear) && result) {
                year = y;
                valid |= ScRecord::ValidYear;
                change = true;
            }

            int s;
            result = tryGetSeason(str, &s);
            if (!isValid(ScRecord::ValidSeason) && result) {
                season = s;
                valid |= ScRecord::ValidSeason;
                change = true;
            }

            ScRecord::Game g;
            result = classifier.tryGetGameFromEvent(str, &g, &str);
            if (!isValid(ScRecord::ValidGame) && result) {
                game = g;
                valid |= ScRecord::ValidGame;
                change = true;
            }

//            if (isValid(ScRecord::ValidGame)) {
//                classifier.cleanEvent((Game)game, str);
//            }

            if (!isValid(ValidEventName) || str.size() < eventName.size()) {
                removeCrud(str);
                eventName = str;
                valid |= ScRecord::ValidEventName;
                change = true;
            }
        }

        if (isValid(ValidStageName)) {
            ScRecord::Game g;
            if (!isValid(ScRecord::ValidGame) &&
                    classifier.tryGetGameFromStage(stage, &g, nullptr)) {
                game = g;
                valid |= ScRecord::ValidGame;
                change = true;
            }
        }

        if (isValid(ValidSides)) {
            auto str = side1Name + QStringLiteral(" vs ") + side2Name;

            ScRecord::Game g;
            if (!isValid(ScRecord::ValidGame) &&
                    classifier.tryGetGameFromMatch(str, &g, nullptr)) {
                game = g;
                valid |= ScRecord::ValidGame;
                change = true;
            }
        }

        if (isValid(ValidMatchName)) {
            ScRecord::Game g;
            if (!isValid(ScRecord::ValidGame) &&
                    classifier.tryGetGameFromMatch(matchName, &g, nullptr)) {
                game = g;
                valid |= ScRecord::ValidGame;
                change = true;
            }

            if (!isValid(ValidSides) && tryGetSides(matchName, &side1Name, &side2Name)) {
                valid |= ScRecord::ValidSides;
                change = true;
            }

            if (!isValid(ValidMatchNumber) && tryGetMatchNumber(matchName, &matchNumber)) {
                valid |= ScRecord::ValidMatchNumber;
                change = true;
            }
        }
    }
}

//void ScRecord::parseEvent(const QString& input) {
//    QString str = input;
//    str.replace(newLines, QStringLiteral(" "));

//    if (!isValid(ScRecord::ValidGame) && tryGetGame(str, &game)) {
//        valid |= ScRecord::ValidGame;
//    }

//    if (!isValid(ScRecord::ValidSeason) && tryGetSeason(str, &season)) {
//        valid |= ScRecord::ValidSeason;
//    }

//    if (!isValid(ScRecord::ValidYear) && tryGetYear(str, &year)) {
//        valid |= ScRecord::ValidYear;
//    }

//    QDate date;
//    if (!isValid(ScRecord::ValidYear) && tryGetDate(str, &date)) {
//        year = date.year();
//        valid |= ScRecord::ValidYear;
//    }

//    removeCrud(str);
//    eventFullName = str;
//}

//void ScRecord::resolveStage(const QString& str) {
//    if ((valid & ValidGame) == 0) {
//        int index = broodwarStageIsGame.indexIn(str);
//        if (index >= 0) {
//            valid |= ValidGame;
//            game = GameBroodWar;
//        }
//    }
//}

//void ScRecord::resolveMatch(const QString& str) {
//    if ((valid & ValidGame) == 0) {
//        int index = sc2MatchIsGame.indexIn(str);
//        if (index >= 0) {
//            valid |= ValidGame;
//            game = GameSc2;
//        }

//        index = broodWarMatchIsGame.indexIn(str);
//        if (index >= 0) {
//            valid |= ValidGame;
//            game = GameBroodWar;
//        }
//    }
//}

QDomNode ScRecord::toXml(QDomDocument& doc) const {
    auto element = doc.createElement(QStringLiteral("record"));
    element.setAttribute(QStringLiteral("version"), 1);

    if (isValid(ValidGame)) {
        element.setAttribute(QStringLiteral("game"), game);
    }

    if (isValid(ValidYear)) {
        element.setAttribute(QStringLiteral("year"), year);
    }

    if (isValid(ValidSeason)) {
        element.setAttribute(QStringLiteral("season"), season);
    }

    if (isValid(ValidEventName)) {
        element.setAttribute(QStringLiteral("event_name"), eventName);
    }

    if (isValid(ValidEventFullName)) {
        element.setAttribute(QStringLiteral("event_full_name"), eventFullName);
    }

    if (isValid(ValidStageName)) {
        element.setAttribute(QStringLiteral("stage"), stage);
    }

    if (isValid(ValidMatchDate)) {
        element.setAttribute(QStringLiteral("match_date"), matchDate.toString(Qt::ISODate));
    }

    if (isValid(ValidSides)) {
        element.setAttribute(QStringLiteral("side1"), side1Name);
        element.setAttribute(QStringLiteral("side2"), side2Name);
    }

    if (isValid(ValidRaces)) {
        element.setAttribute(QStringLiteral("race1"), side1Race);
        element.setAttribute(QStringLiteral("race2"), side2Race);
    }

    if (isValid(ValidUrl)) {
        element.setAttribute(QStringLiteral("url"), url);
    }

    if (isValid(ValidMatchName)) {
        element.setAttribute(QStringLiteral("match_name"), matchName);
    }

    return element;
}

ScRecord
ScRecord::fromXml(const QDomNode& node) {

    ScRecord record;

    auto attrs = node.attributes();
    auto version = attrs.namedItem(QStringLiteral("version")).nodeValue().toInt();
    if (version == 1) {
        auto str = attrs.namedItem(QStringLiteral("event_full_name")).nodeValue();
        if (!str.isEmpty()) {
            record.eventFullName = str;
            record.valid |= ScRecord::ValidEventFullName;
        }
        str = attrs.namedItem(QStringLiteral("game")).nodeValue();
        if (!str.isEmpty()) {
            record.game = str.toInt();
            record.valid |= ScRecord::ValidGame;
        }

        str = attrs.namedItem(QStringLiteral("year")).nodeValue();
        if (!str.isEmpty()) {
            record.year = str.toInt();
            record.valid |= ScRecord::ValidYear;
        }

        str = attrs.namedItem(QStringLiteral("season")).nodeValue();
        if (!str.isEmpty()) {
            record.season = str.toInt();
            record.valid |= ScRecord::ValidSeason;
        }

        str = attrs.namedItem(QStringLiteral("event_name")).nodeValue();
        if (!str.isEmpty()) {
            record.eventName = str;
            record.valid |= ScRecord::ValidEventName;
        }

        str = attrs.namedItem(QStringLiteral("stage")).nodeValue();
        if (!str.isEmpty()) {
            record.stage = str;
            record.valid |= ScRecord::ValidStageName;
        }

        str = attrs.namedItem(QStringLiteral("match_date")).nodeValue();
        if (!str.isEmpty()) {
            record.matchDate = QDate::fromString(str, Qt::ISODate);
            record.valid |= ScRecord::ValidMatchDate;
        }

        str = attrs.namedItem(QStringLiteral("side1")).nodeValue();
        if (!str.isEmpty()) {
            record.side1Name = str;
            record.side2Name = attrs.namedItem(QStringLiteral("side2")).nodeValue();
            record.valid |= ScRecord::ValidSides;
        }

        str = attrs.namedItem(QStringLiteral("race1")).nodeValue();
        if (!str.isEmpty()) {
            record.side1Race = str.toInt();
            record.side2Race = attrs.namedItem(QStringLiteral("race2")).nodeValue().toInt();
            record.valid |= ScRecord::ValidRaces;
        }

        str = attrs.namedItem(QStringLiteral("url")).nodeValue();
        if (!str.isEmpty()) {
            record.url = str;
            record.valid |= ScRecord::ValidUrl;
        }

        str = attrs.namedItem(QStringLiteral("match_name")).nodeValue();
        if (!str.isEmpty()) {
            record.matchName = str;
            record.valid |= ScRecord::ValidMatchName;
        }
    }

    return record;
}

QDebug operator<<(QDebug debug, const ScRecord& record) {
    QDebugStateSaver saver(debug);
    debug.nospace() << "ScRecord(\n";
    if (record.isValid(ScRecord::ValidEventFullName)) {
        debug.nospace() << "\teventFullName=" << record.eventFullName << "\n";
    }

    if (record.isValid(ScRecord::ValidEventName)) {
        debug.nospace() << "\teventName=" << record.eventName << "\n";
    }

    if (record.isValid(ScRecord::ValidGame)) {
        debug.nospace() << "\tgame=" << record.game << "\n";
    }

    if (record.isValid(ScRecord::ValidMatchDate)) {
        debug.nospace() << "\tmatchDate=" << record.matchDate << "\n";
    }

    if (record.isValid(ScRecord::ValidMatchName)) {
        debug.nospace() << "\tmatchName=" << record.matchName << "\n";
    }

    if (record.isValid(ScRecord::ValidRaces)) {
        debug.nospace() << "\traces=" << record.side1Race << "/" << record.side2Race << "\n";
    }

    if (record.isValid(ScRecord::ValidSeason)) {
        debug.nospace() << "\tseason=" << record.season << "\n";
    }

    if (record.isValid(ScRecord::ValidSides)) {
        debug.nospace() << "\tsides=" << record.side1Name << "/" << record.side2Name << "\n";
    }

    if (record.isValid(ScRecord::ValidStageName)) {
        debug.nospace() << "\tstage=" << record.stage << "\n";
    }

    if (record.isValid(ScRecord::ValidUrl)) {
        debug.nospace() << "\turl=" << record.url << "\n";
    }

    if (record.isValid(ScRecord::ValidYear)) {
        debug.nospace() << "\tyear=" << record.year << "\n";
    }

    if (record.isValid(ScRecord::ValidStageRank)) {
        debug.nospace() << "\tstageRank=" << record.stageRank << "\n";
    }

    debug.nospace() << ")\n";
    return debug;
}
