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

#include <QtTest/QtTest>

#include <QDebug>
#include <QFile>
#include <cstdio>

using namespace  std;



#include <ScRecord.h>
#include <ScClassifier.h>



class TestParser : public QObject {
    Q_OBJECT
public:
    TestParser();
private slots:
    void autocompleteFromEvent();
    void autocompleteFromEvent_data();
    void autocompleteFromStage();
    void autocompleteFromStage_data();
    void autocompleteFromMatch();
    void autocompleteFromMatch_data();
    void autocomplete();
    void autocomplete_data();
private:
    ScClassifier classifier;
};

#define make(a) a

TestParser::TestParser() {

    QVERIFY(classifier.load(QStringLiteral(STARFISH_DIR "/src/starfish-app/classifier.json")));
}

void
TestParser::autocompleteFromEvent_data() {
//    QTest::addColumn<QString>("input");
    QTest::addColumn<ScRecord>("record"); // the string must match the token in the QFETCH macro



    ScRecord record;

#if 1

    record.valid =  ScRecord::ValidYear | ScRecord::ValidGame;
    record.eventName = "IEM Kiev";
    record.eventFullName = "IEM Kiev January 22nd 2012";
    record.year = 2012;
    record.game = ScRecord::GameSc2;
    QTest::newRow(record.eventFullName.toLocal8Bit()) << record;


    record.valid =  ScRecord::ValidYear | ScRecord::ValidSeason | ScRecord::ValidGame;
    record.eventName = "GSTL";
    record.eventFullName = "GSTL Season 1 2011 October 8th 2011";
    record.season = 1;
    record.year = 2011;
    record.game = ScRecord::GameSc2;
    QTest::newRow(record.eventFullName.toLocal8Bit()) << record;


    record.valid =  ScRecord::ValidYear | ScRecord::ValidGame;
    record.eventName = "DreamHack Valencia";
    record.eventFullName = "DreamHack Valencia 2011 September 17th 2011";
    record.year = 2011;
    record.game = ScRecord::GameSc2;
    QTest::newRow(record.eventFullName.toLocal8Bit()) << record;

    record.valid =  ScRecord::ValidYear | ScRecord::ValidGame;
    record.eventName = "DreamHack Open Tours";
    record.eventFullName = "DreamHack Open: Tours Febuary 1st 2015";
    record.year = 2015;
    record.game = ScRecord::GameSc2;
    QTest::newRow(record.eventFullName.toLocal8Bit()) << record;


    record.valid = ScRecord::ValidYear | ScRecord::ValidSeason | ScRecord::ValidGame;
    record.eventName = "StarLeague";
    record.eventFullName = "StarCraft II StarLeague S2 May 21st 2015";
    record.year = 2015;
    record.season = 2;
    record.game = ScRecord::GameSc2;
    QTest::newRow(record.eventFullName.toLocal8Bit()) << record;

    record.valid = ScRecord::ValidYear | ScRecord::ValidSeason | ScRecord::ValidGame;
    record.eventName = "HomeStory Cup";
    record.eventFullName = "HomeStory Cup XI Febuary 11th 2015";
    record.year = 2015;
    record.season = 11;
    record.game = ScRecord::GameSc2;
    QTest::newRow(record.eventFullName.toLocal8Bit()) << record;

    record.valid =  ScRecord::ValidYear | ScRecord::ValidSeason | ScRecord::ValidGame;
    record.eventName = "IEM Shenzhen";
    record.eventFullName = "IEM Season X - Shenzhen December 3rd 2015";
    record.year = 2015;
    record.season = 10;
    record.game = ScRecord::GameSc2;
    QTest::newRow(record.eventFullName.toLocal8Bit()) << record;

    record.valid =  ScRecord::ValidYear | ScRecord::ValidSeason | ScRecord::ValidGame;
    record.eventName = "Afreeca Starleague";
    record.eventFullName = "Afreeca Starleague S5 (BW) April 3rd 2018";
    record.year = 2018;
    record.season = 5;
    record.game = ScRecord::GameBroodWar;
    QTest::newRow(record.eventFullName.toLocal8Bit()) << record;


    record.valid =  ScRecord::ValidYear | ScRecord::ValidGame;
    record.eventName = "Olimoleague";
    record.eventFullName = "Olimoleague April 5th 2018";
    record.year = 2018;
    record.game = ScRecord::GameSc2;
    QTest::newRow(record.eventFullName.toLocal8Bit()) << record;

    record.valid =  ScRecord::ValidYear | ScRecord::ValidGame | ScRecord::ValidSeason;
    record.eventName = "NationWars";
    record.eventFullName = "NationWars V April 5th 2018";
    record.year = 2018;
    record.season = 5;
    record.game = ScRecord::GameSc2;
    QTest::newRow(record.eventFullName.toLocal8Bit()) << record;

    record.valid = ScRecord::ValidGame | ScRecord::ValidSeason;
    record.eventName = "Nation Wars";
    record.eventFullName = "Nation Wars 4";
    record.season = 4;
    record.game = ScRecord::GameSc2;
    QTest::newRow(record.eventFullName.toLocal8Bit()) << record;


    record.valid =  ScRecord::ValidGame;
    record.eventName = "Go4SC2 Cup";
    record.eventFullName = "Go4SC2 Cup";
    record.game = ScRecord::GameSc2;
    QTest::newRow(record.eventFullName.toLocal8Bit()) << record;

    record.valid =  ScRecord::ValidGame;
    record.eventName = "OCS Masters";
    record.eventFullName = "OCS Masters";
    record.game = ScRecord::GameSc2;
    QTest::newRow(record.eventFullName.toLocal8Bit()) << record;

    record.valid =  ScRecord::ValidGame;
    record.eventName = "100 Friday";
    record.eventFullName = "100 Friday";
    record.game = ScRecord::GameSc2;
    QTest::newRow(record.eventFullName.toLocal8Bit()) << record;

    record.valid =  ScRecord::ValidGame | ScRecord::ValidYear;
    record.eventName = "BSTL Special";
    record.eventFullName = "BSTL 2017 Special ";
    record.game = ScRecord::GameSc2;
    record.year = 2017;
    QTest::newRow(record.eventFullName.toLocal8Bit()) << record;


    record.valid =  ScRecord::ValidGame;
    record.eventName = "Corsair Cup";
    record.eventFullName = "Corsair Cup";
    record.game = ScRecord::GameSc2;
    QTest::newRow(record.eventFullName.toLocal8Bit()) << record;

    record.valid =  ScRecord::ValidGame;
    record.eventName = "Ariel Cup";
    record.eventFullName = "Ariel Cup";
    record.game = ScRecord::GameSc2;
    QTest::newRow(record.eventFullName.toLocal8Bit()) << record;

    record.valid =  ScRecord::ValidGame;
    record.eventName = "Kung fu Cup";
    record.eventFullName = "Kung fu Cup";
    record.game = ScRecord::GameSc2;
    QTest::newRow(record.eventFullName.toLocal8Bit()) << record;

    record.valid = ScRecord::ValidGame;
    record.eventName = "I love StarCraft";
    record.eventFullName = "I love StarCraft";
    record.game = ScRecord::GameSc2;
    QTest::newRow(record.eventFullName.toLocal8Bit()) << record;

    record.valid = ScRecord::ValidGame;
    record.eventName = "NASL Open";
    record.eventFullName = "NASL Open";
    record.game = ScRecord::GameSc2;
    QTest::newRow(record.eventFullName.toLocal8Bit()) << record;

    record.valid = ScRecord::ValidGame | ScRecord::ValidSeason;
    record.eventName = "NASL";
    record.eventFullName = "NASL 4";
    record.game = ScRecord::GameSc2;
    record.season = 4;
    QTest::newRow(record.eventFullName.toLocal8Bit()) << record;

    record.valid = ScRecord::ValidGame | ScRecord::ValidYear;
    record.eventName = "Proleague";
    record.eventFullName = "Proleague (2012-2013)";
    record.game = ScRecord::GameSc2;
    record.year = 2012;
    QTest::newRow(record.eventFullName.toLocal8Bit()) << record;

    record.valid = ScRecord::ValidGame | ScRecord::ValidYear;
    record.eventName = "Proleague Preseason";
    record.eventFullName = "2014 Proleague Preseason";
    record.game = ScRecord::GameSc2;
    record.year = 2014;
    QTest::newRow(record.eventFullName.toLocal8Bit()) << record;

    record.valid = ScRecord::ValidGame;
    record.eventName = "Iron Squid";
    record.eventFullName = "Iron Squid";
    record.game = ScRecord::GameSc2;
    QTest::newRow(record.eventFullName.toLocal8Bit()) << record;

    record.valid = ScRecord::ValidGame;
    record.eventName = "Bronze League Heroes";
    record.eventFullName = "Bronze League Heroes";
    record.game = ScRecord::GameSc2;
    QTest::newRow(record.eventFullName.toLocal8Bit()) << record;

    record.valid = 0;
    record.eventName = "POSTPWN $1000";
    record.eventFullName = "POSTPWN $1000";
    QTest::newRow(record.eventFullName.toLocal8Bit()) << record;

    record.valid = ScRecord::ValidGame;
    record.eventName = "Vortex Championship";
    record.eventFullName = "Vortex Championship";
    record.game = ScRecord::GameSc2;
    QTest::newRow(record.eventFullName.toLocal8Bit()) << record;

    record.valid = ScRecord::ValidGame | ScRecord::ValidSeason;
    record.eventName = "ASL Prime";
    record.eventFullName = "ASL Season 4 Prime";
    record.season = 4;
    record.game = ScRecord::GameSc2;
    QTest::newRow(record.eventFullName.toLocal8Bit()) << record;

    record.valid = ScRecord::ValidGame | ScRecord::ValidSeason | ScRecord::ValidYear;
    record.eventName = "StarLeague";
    record.eventFullName = "2015 StarLeague S2";
    record.season = 2;
    record.game = ScRecord::GameSc2;
    record.year = 2015;
    QTest::newRow(record.eventFullName.toLocal8Bit()) << record;


    record.valid = ScRecord::ValidGame;
    record.eventName = "Legacy of the Void Beta";
    record.eventFullName = "Legacy of the Void Beta";
    record.game = ScRecord::GameSc2;
    QTest::newRow(record.eventFullName.toLocal8Bit()) << record;

    record.valid = ScRecord::ValidGame;
    record.eventName = "Drunken Cast";
    record.eventFullName = "Drunken Cast";
    record.game = ScRecord::GameSc2;
    QTest::newRow(record.eventFullName.toLocal8Bit()) << record;

    record.valid = ScRecord::ValidGame;
    record.eventName = "VSL Team League";
    record.eventFullName = "VSL Team League";
    record.game = ScRecord::GameSc2;
    QTest::newRow(record.eventFullName.toLocal8Bit()) << record;
#endif
    record.valid = ScRecord::ValidGame | ScRecord::ValidYear | ScRecord::ValidSeason;
    record.eventName = "ASL Team Battle";
    record.eventFullName = "2017 ASL Team Battle S1";
    record.game = ScRecord::GameBroodWar;
    record.year = 2017;
    record.season = 1;
    QTest::newRow(record.eventFullName.toLocal8Bit()) << record;

    record.valid = ScRecord::ValidGame;
    record.eventName = "17173.com World Cup";
    record.eventFullName = "17173.com Starcraft 2 World Cup";
    record.game = ScRecord::GameSc2;
    QTest::newRow(record.eventFullName.toLocal8Bit()) << record;

    record.valid = ScRecord::ValidGame | ScRecord::ValidSeason | ScRecord::ValidYear;
    record.eventName = "StarLeague Classic";
    record.eventFullName = "2017 StarLeague S1 Classic";
    record.game = ScRecord::GameBroodWar;
    record.year = 2017;
    record.season = 1;
    QTest::newRow(record.eventFullName.toLocal8Bit()) << record;
}


void
TestParser::autocompleteFromEvent() {
//    QFETCH(QString, input);
    QFETCH(ScRecord, record);


    ScRecord completed;
    completed.valid = ScRecord::ValidEventFullName;
    completed.eventFullName = record.eventFullName;
    completed.autoComplete(classifier);

    QVERIFY(completed.valid & ScRecord::ValidEventName);
    QCOMPARE(completed.eventName, record.eventName);

    if (record.valid & ScRecord::ValidYear) {
        QCOMPARE(completed.year, record.year);
    }

    if (record.valid & ScRecord::ValidSeason) {
        QCOMPARE(completed.season, record.season);
    }

    if (record.valid & ScRecord::ValidGame) {
        QCOMPARE(completed.game, record.game);
    }
}


void
TestParser::autocompleteFromStage_data() {
//    QTest::addColumn<QString>("input");
    QTest::addColumn<ScRecord>("record"); // the string must match the token in the QFETCH macro



    ScRecord record;


    record.valid = ScRecord::ValidGame;
    record.stage = "Pro SC2 VOD";
    record.game = ScRecord::GameSc2;
    QTest::newRow(record.stage.toLocal8Bit()) << record;

    record.valid = ScRecord::ValidGame;
    record.stage = "lets learn starcraft";
    record.game = ScRecord::GameBroodWar;
    QTest::newRow(record.stage.toLocal8Bit()) << record;

    record.valid = ScRecord::ValidGame;
    record.stage = "vvv gaming";
    record.game = ScRecord::GameSc2;
    QTest::newRow(record.stage.toLocal8Bit()) << record;
}

void
TestParser::autocompleteFromStage() {
//    QFETCH(QString, input);
    QFETCH(ScRecord, record);


    ScRecord completed;
    completed.valid = ScRecord::ValidStage;
    completed.stage = record.stage;
    completed.autoComplete(classifier);

    QVERIFY(completed.valid & ScRecord::ValidStage);
    QCOMPARE(completed.stage, record.stage);

    if (record.valid & ScRecord::ValidYear) {
        QCOMPARE(completed.year, record.year);
    }

    if (record.valid & ScRecord::ValidSeason) {
        QCOMPARE(completed.season, record.season);
    }

    if (record.valid & ScRecord::ValidGame) {
        QCOMPARE(completed.game, record.game);
    }
}


void
TestParser::autocompleteFromMatch_data() {
//    QTest::addColumn<QString>("input");
    QTest::addColumn<ScRecord>("record"); // the string must match the token in the QFETCH macro



    ScRecord record;


    record.valid = ScRecord::ValidGame;
    record.matchName = "JINAIR - CJ Entus";
    record.game = ScRecord::GameSc2;
    QTest::newRow(record.matchName.toLocal8Bit()) << record;

    record.valid = ScRecord::ValidGame;
    record.matchName = "JonSnow - DnS";
    record.game = ScRecord::GameSc2;
    QTest::newRow(record.matchName.toLocal8Bit()) << record;

    record.valid = ScRecord::ValidGame;
    record.matchName = "Hyun - BabyKnight";
    record.game = ScRecord::GameSc2;
    QTest::newRow(record.matchName.toLocal8Bit()) << record;

    record.valid = ScRecord::ValidGame;
    record.matchName = "SLush - Psy";
    record.game = ScRecord::GameSc2;
    QTest::newRow(record.matchName.toLocal8Bit()) << record;

    record.valid = ScRecord::ValidGame;
    record.matchName = "Jaedong - fantasy";
    record.game = ScRecord::GameBroodWar;
    QTest::newRow(record.matchName.toLocal8Bit()) << record;

    record.valid = ScRecord::ValidGame;
    record.matchName = "cure - soo";
    record.game = ScRecord::GameSc2;
    QTest::newRow(record.matchName.toLocal8Bit()) << record;

    record.valid = ScRecord::ValidGame;
    record.matchName = "Apocalypse - GunGfuBanDa";
    record.game = ScRecord::GameSc2;
    QTest::newRow(record.matchName.toLocal8Bit()) << record;

    record.valid = ScRecord::ValidGame;
    record.matchName = "ret - StrinterN";
    record.game = ScRecord::GameSc2;
    QTest::newRow(record.matchName.toLocal8Bit()) << record;
}

void
TestParser::autocompleteFromMatch() {
//    QFETCH(QString, input);
    QFETCH(ScRecord, record);


    ScRecord completed;
    completed.valid = ScRecord::ValidMatchName;
    completed.matchName = record.matchName;
    completed.autoComplete(classifier);

    QVERIFY(completed.valid & ScRecord::ValidMatchName);
    QCOMPARE(completed.matchName, record.matchName);

    if (record.valid & ScRecord::ValidYear) {
        QCOMPARE(completed.year, record.year);
    }

    if (record.valid & ScRecord::ValidSeason) {
        QCOMPARE(completed.season, record.season);
    }

    if (record.valid & ScRecord::ValidGame) {
        QCOMPARE(completed.game, record.game);
    }
}



void
TestParser::autocomplete_data() {
//    QTest::addColumn<QString>("input");
    QTest::addColumn<ScRecord>("record"); // the string must match the token in the QFETCH macro



    ScRecord record;


    record.valid = ScRecord::ValidGame | ScRecord::ValidEventFullName |
            ScRecord::ValidStage | ScRecord::ValidYear |
            ScRecord::ValidMatchDate | ScRecord::ValidMatchName |
            ScRecord::ValidEventName | ScRecord::ValidSides;
    record.matchName = "Stats - CoCa";
    record.matchDate = QDate(2013, 12, 16);
    record.game = ScRecord::GameSc2;
    record.stage = "Pro SC2 VOD";
    record.eventFullName = "2014 Proleague Preseason";
    record.eventName = "Proleague Preseason";
    record.year = 2014;
    record.side1Name = "Stats";
    record.side2Name = "CoCa";
    QTest::newRow(record.eventFullName.toLocal8Bit()) << record;

    record.valid = ScRecord::ValidGame | ScRecord::ValidEventFullName |
            ScRecord::ValidStage | ScRecord::ValidYear |
            ScRecord::ValidMatchDate | ScRecord::ValidMatchName |
            ScRecord::ValidEventName | ScRecord::ValidSides | ScRecord::ValidSeason;
    record.matchName = "Soulkey vs free";
    record.matchDate = QDate(2017,05,26);
    record.game = ScRecord::GameBroodWar;
    record.stage = "Day 7";
    record.eventFullName = "2017 StarLeague S1 Classic";
    record.eventName = "StarLeague Classic";
    record.year = 2017;
    record.side1Name = "Soulkey";
    record.side2Name = "free";
    record.season = 1;
    QTest::newRow(record.eventFullName.toLocal8Bit()) << record;
}

void
TestParser::autocomplete() {
//    QFETCH(QString, input);
    QFETCH(ScRecord, record);


    ScRecord completed;
    completed.valid = ScRecord::ValidMatchName | ScRecord::ValidStage | ScRecord::ValidEventFullName | ScRecord::ValidMatchDate;
    completed.matchName = record.matchName;
    completed.matchDate = record.matchDate;
    completed.stage = record.stage;
    completed.eventFullName = record.eventFullName;
    completed.autoComplete(classifier);

    QVERIFY(completed.valid & ScRecord::ValidMatchName);
    QVERIFY(completed.valid & ScRecord::ValidMatchDate);
    QVERIFY(completed.valid & ScRecord::ValidStage);
    QVERIFY(completed.valid & ScRecord::ValidEventFullName);
    QVERIFY(completed.valid & ScRecord::ValidEventName);
    QCOMPARE(completed.matchName, record.matchName);
    QCOMPARE(completed.matchDate, record.matchDate);
    QCOMPARE(completed.stage, record.stage);
    QCOMPARE(completed.eventFullName, record.eventFullName);
    QCOMPARE(completed.eventName, record.eventName);

    if (record.valid & ScRecord::ValidYear) {
        QCOMPARE(completed.year, record.year);
    }

    if (record.valid & ScRecord::ValidSeason) {
        QCOMPARE(completed.season, record.season);
    }

    if (record.valid & ScRecord::ValidGame) {
        QCOMPARE(completed.game, record.game);
    }

    if (record.valid & ScRecord::ValidSides) {
        QCOMPARE(completed.side1Name, record.side1Name);
        QCOMPARE(completed.side2Name, record.side2Name);
    }
}

QTEST_APPLESS_MAIN(TestParser)
#include "TestParser.moc"
/*
static const char* const s_Data[] = {
"GSL Super TournamentApril 8th 2018"
"NationWars VApril 7th 2018"
"BeastyqtSC2April 4th 2018"
"Crank SC2April 4th 2018"
"WinterStarcraftApril 4th 2018
"LowkoTVApril 4th 2018
"PiGApril 4th 2018
"Afreeca Starleague S5 (BW)April 3rd 2018
"NeuroApril 2nd 2018
"GSL Season One Code SMarch 31st 2018
"StarCraft 20th AnniversaryMarch 31st 2018
"Falcon PaladinMarch 26th 2018
"WESGMarch 18th 2018
"WardiTVMarch 18th 2018
"Day9TVMarch 10th 2018
"BaseTradeTVMarch 8th 2018
"IEM Season XII - World ChampionshipMarch 4th 2018
"IEM Season XII - PyeongChangFebruary 7th 2018
"OlimoleagueFebruary 5th 2018
"WCS LeipzigJanuary 28th 2018
"Falcon Paladin 2017December 31st 2017
"WardiTV 2017December 31st 2017
"LowkoTV 2017December 31st 2017
"BeastyqtSC2 2017December 31st 2017
"Crank SC2 2017December 30th 2017
"WinterStarcraft 2017December 30th 2017
"Twitch StarCraft Holiday BashDecember 25th 2017
"Day9TV 2017December 24th 2017
"BaseTradeTV 2017December 23rd 2017
"Natural ExpansionDecember 13th 2017
"ZOTAC CUP MastersDecember 3rd 2017
"THE PATCHNovember 28th 2017
"TL OpenNovember 27th 2017
"SHOUTcraft KingsNovember 20th 2017
"G-Star WEGL Super FightNovember 19th 2017
"Afreeca Starleague S4 (BW)November 12th 2017
"HomeStory Cup XVINovember 12th 2017
"WCS Global FinalsNovember 4th 2017
"PiG DailyOctober 28th 2017
"Seoul Cup - OGN Super MatchOctober 14th 2017
"GSL Super Tournament 2017October 1st 2017
"Starleague Season 2September 24th 2017
"GSL Season Three Code SSeptember 16th 2017
"WCS MontrealSeptember 11th 2017
"Olimoleague 2017September 10th 2017
"WardiTV 2016September 7th 2017
"The ShowAugust 27th 2017
"Starcraft Remastered Launch EventAugust 16th 2017
"GSL vs The WorldAugust 9th 2017
"IEM Season XII - ShanghaiJuly 30th 2017
"Homestory Cup XVJuly 23rd 2017
"Olimoleague 2016July 22nd 2017
"WCS ValenciaJuly 15th 2017
"EsportsEarnings Casters InvitationalJuly 9th 2017
"GSL Season Two Code SJune 24th 2017
"StarLeague Season 1June 22nd 2017
"WCS JonkopingJune 19th 2017
"SSL Classic S1 (BW)June 10th 2017
"Afreeca Starleague S3 (BW)June 4th 2017
"Polt Ladder GamesMay 8th 2017
"WCS AustinApril 30th 2017
"AfreecaTV GSL Super Tournament 1April 9th 2017
"GSL Season One Code S 2017March 26th 2017
"ASL Team Battle S1March 20th 2017
"IEM Season XI - World ChampionshipMarch 5th 2017
"GSL Loser Strikes BackFebruary 2nd 2017
"Afreeca Starleague S2 (BW)January 22nd 2017
"NationWars IVJanuary 21st 2017
"The BunkerJanuary 21st 2017
"WESG FinalsJanuary 15th 2017
"BaseTradeTV LOTVDecember 21st 2016
"Falcon Paladin LoTV SeriesDecember 21st 2016
"IEM Season XI - GyeonggiDecember 18th 2016
"PiG Daily 2016December 16th 2016
"SHOUTcraft Kings 2016December 12th 2016
"HomeStory Cup XIVNovember 20th 2016
"WESG Asia PacificNovember 12th 2016
"WCS Global Playoffs and FinalsNovember 5th 2016
"WESG AmericasOctober 24th 2016
"TaKes Penthouse PartyOctober 21st 2016
"Falcon Paladin LoTV Series 2015October 16th 2016
"WESG 2016October 10th 2016
"Kespa CupOctober 3rd 2016
"WCS Korean CrossFinals 2September 25th 2016
"StarLeague S2 (SSL)September 11th 2016
"GSL Season TwoSeptember 10th 2016
"Afreeca Starleague (BW)September 10th 2016
"ProleagueSeptember 3rd 2016
"DreamHack Open MontrealAugust 14th 2016
"Crank LOTVAugust 2nd 2016
"IEM Season XI ShanghaiJuly 30th 2016
"DreamHack ValenciaJuly 16th 2016
"Natural Expansion 2016July 11th 2016
"OVERWATCH: Atlantic ShowdownJuly 8th 2016
"OVERWATCH: GosuGamers NA 14July 3rd 2016
"OVERWATCH: GosuGamers EU 14July 2nd 2016
"JoRoSaR YouTubeJune 27th 2016
"HomeStory Cup XIIIJune 26th 2016
"OVERWATCH: ONOG Operation BreakoutJune 26th 2016
"OVERWATCH: OG InvitationalJune 25th 2016
"OVERWATCH: Alienware Monthly Melee JuneJune 23rd 2016
"OVERWATCH: GosuGamers NA 13June 19th 2016
"OVERWATCH: GosuGamers EU 13June 18th 2016
"Happy LoTVJune 17th 2016
"Polt LoTV LadderJune 9th 2016
"OVERWATCH: GosuGamers NA 11June 5th 2016
"OVERWATCH: Agents RisingMay 30th 2016
"WCS Korea CrossFinalsMay 22nd 2016
"WCS Spring Circuit ChampionshipMay 16th 2016
"DreamHack Open: AustinMay 8th 2016
"Kung Fu CupMay 4th 2016
"GSL Season OneMay 1st 2016
"The Late GameMay 1st 2016
"BosstossTVApril 13th 2016
"StarLeague (SSL)April 9th 2016
"GPL InternationalMarch 27th 2016
"ChanmanV - UnfilteredMarch 25th 2016
"WCS Winter Circuit Championship - IEM KatowiceMarch 5th 2016
"WCS NA QualifierFebruary 14th 2016
"Kings of the CraftFebruary 7th 2016
"IEM Season X - TaipeiFebruary 2nd 2016
"The Late Game 2015January 28th 2016
"RemaxJanuary 27th 2016
"Remax 2015January 27th 2016
""DreamHack Open: LeipzigJanuary 24th 2016
"VANT36.5 National Starleague (BW)January 23rd 2016
"The Evolution ChamberJanuary 20th 2016
"NationWars IIIJanuary 3rd 2016
"UnfilteredDecember 31st 2015
"GSL Preseason Week 2December 26th 2015
"Polt LoTV Ladder GamesDecember 20th 2015
"HomeStory Cup XIIDecember 20th 2015
GSL Preseason Week 1December 18th 2015
ROOT Gaming WW3December 13th 2015
Remax 2014December 8th 2015
DreamHack LoTV ChampionshipNovember 28th 2015
BaseTradeTvTNovember 21st 2015
Archon All StarsNovember 15th 2015
SanDisk ShoutCraft InvitationalNovember 15th 2015
WCS Global Finals 2015November 7th 2015
DreamHack ROCCAT LotVOctober 18th 2015
SK Telecom ProleagueOctober 10th 2015
GSL Season 3 (Twitch)October 4th 2015
Bronze to MastersOctober 1st 2015
DreamHack Open: StockholmSeptember 25th 2015
GSL Season 3September 24th 2015
StarCraft II StarLeague S3September 20th 2015
Red Bull BG: WashingtonSeptember 19th 2015
SC2 Up and ComingSeptember 15th 2015
WCS Season 3September 13th 2015
Red Bull BG: Santa MonicaAugust 30th 2015
Hell, Its Aboot TimeAugust 22nd 2015
IEM Season X - GamescomAugust 9th 2015
SHOUTcraft Clan Wars S2July 23rd 2015
IEM Season X - ShenzhenJuly 19th 2015
DreamHack Open: ValenciaJuly 17th 2015
KeSPA Cup S2July 12th 2015
Rocket Beans Archon CupJuly 12th 2015
HomeStory Cup XIJuly 5th 2015
WCS Season 2June 28th 2015
Red Bull BG: TorontoJune 28th 2015
GSL Season 2June 28th 2015
GSL Season 2 (Twitch)June 28th 2015
StarCraft II StarLeague S2June 20th 2015
Red Bull BG: ShowmatchesJune 17th 2015
Legacy of the UltrasJune 11th 2015
Go4SC2 MayJune 9th 2015
OlimoLeague JuneJune 6th 2015
DSCL OpenJune 4th 2015
HTC: Best of 69June 2nd 2015
AHGL Season 5June 1st 2015
Jord InvitationalMay 22nd 2015
DreamHack Open: ToursMay 9th 2015
KeSPA Cup S1May 5th 2015
Go4SC2 MarchMay 5th 2015
Gfinity Spring Masters IIMay 3rd 2015
LotV OlimoLeague AprilApril 12th 2015
desRow Weekly AprilApril 11th 2015
Husky StarcraftApril 9th 2015
Lycan League AprilApril 6th 2015
WCS Season 1April 5th 2015
BaseTradeTV HoTS SeriesApril 5th 2015
2v2 Ting CupMarch 31st 2015
desRow Weekly MarchMarch 28th 2015
GSL Season 1March 28th 2015
The Next Aiur ChefMarch 24th 2015
Battle of the Ladder HeroesMarch 24th 2015
GSL Season 1 (Twitch)March 22nd 2015
StarCraft II StarLeague S1March 21st 2015
OlimoLeague MarchMarch 17th 2015
IEM IX - World ChampionshipMarch 15th 2015
Two vs Twournament EUMarch 13th 2015
Lycan League Feb FinalsMarch 2nd 2015
TL Map Contest 5 OpenMarch 1st 2015
OlimoLeague FebruaryFebruary 28th 2015
OlimoLeague Week 22February 21st 2015
Two vs Twournament V2.2February 16th 2015
Lycan League February 2February 16th 2015
The Safety NetFebruary 10th 2015
OlimoLeague Week 20February 7th 2015
OlimoLeague Week 21February 7th 2015
Two vs TwournamentFebruary 2nd 2015
IEM Season IX - TaipeiFebruary 1st 2015
WCS AM Season 1January 25th 2015
WCS EU Season 1January 24th 2015
Go4SC2 DecemberJanuary 19th 2015
OlimoLeague Week 19January 17th 2015
Axiom LOTV TournamentJanuary 11th 2015
OlimoLeague Week 18January 10th 2015
Hey, Look, Koreans!January 6th 2015
OlimoLeague Week 17January 4th 2015
Husky Starcraft - HotSDecember 31st 2014
EIZO Holiday BrawlDecember 29th 2014
CaseKing X-Mas CupDecember 21st 2014
OlimoLeague Week 15December 18th 2014
The Fight Before ChristmasDecember 17th 2014
SC2 Up and Coming 2014December 16th 2014
OlimoLeague NovemberDecember 13th 2014
BaseTradeTV HoTS Series 2014December 12th 2014
EPS Germany WinterDecember 12th 2014
Bronze to Masters - MentalityDecember 12th 2014
IEM Season IX - San JoseDecember 7th 2014
Hot6ix CupDecember 7th 2014
KhaldorTV HoTSDecember 6th 2014
Day[9] DailyDecember 4th 2014
DreamHack Open: WinterNovember 29th 2014
32 Boys 1 CupNovember 23rd 2014
OlimoLeague Week 13November 17th 2014
HomeStory Cup XNovember 16th 2014
Madals - HoTS CastsNovember 9th 2014
WCS Global Finals 2014November 8th 2014
OlimoLeague Week 11November 3rd 2014
WECGOctober 21st 2014
The Foreign HopeOctober 15th 2014
OlimoLeague Oct Week 9October 11th 2014
WCS EU Season 3October 5th 2014
OlimoLeague Oct Week 8October 4th 2014
GSL Season 3 (Twitch) 2014October 4th 2014
GSL Season 3 2014October 4th 2014
OlimoLeague Sep FinalsSeptember 29th 2014
DreamHack Open: Stockholm 2014September 27th 2014
Red Bull BG: Washington 2014September 21st 2014
WCS AM Season 3September 17th 2014
DreamHack Open: MoscowSeptember 14th 2014
KeSPA CupSeptember 14th 2014
MSI Beat ITSeptember 12th 2014
MetaSeptember 9th 2014
Whos The Best EuropeanSeptember 7th 2014
IEM Season IX - TorontoAugust 31st 2014
OlimoLeague August FinalsAugust 30th 2014
Unfiltered 2014August 27th 2014
Red Bull BG: DetroitAugust 24th 2014
Live on ThreeAugust 21st 2014
OlimoLeague August Week 3August 16th 2014
Red Bull BG: GlobalAugust 10th 2014
Destiny IAugust 10th 2014
OlimoLeague August Week 2August 9th 2014
SK Telecom Proleague 2014August 9th 2014
Dragon Invitational #4August 8th 2014
DUCKVILLELOL HoTSAugust 8th 2014
OlimoLeague August Week 1August 2nd 2014
The Big OneJuly 25th 2014
IEM Season IX - ShenzhenJuly 20th 2014
DreamHack Open: Valencia 2014July 19th 2014
Red Bull BG: AtlantaJuly 13th 2014
PrOmise - HoTS CastsJuly 9th 2014
BaseTradeTV StarbowJuly 6th 2014
WCS EU Season 2July 6th 2014
WCS AM Season 2July 6th 2014
GSL Season 2 (Twitch) 2014June 28th 2014
GSL Season 2 2014June 28th 2014
Inside The GameJune 24th 2014
MLG AnaheimJune 22nd 2014
Red Bull Battle Grounds: NAJune 19th 2014
DreamHack Open: SummerJune 16th 2014
HomeStory Cup IXJune 8th 2014
SanDisk SHOUTcraftJune 8th 2014
The GD ShowJune 5th 2014
Acer TeamStory Cup S3June 3rd 2014
OGaming Nation Wars IIJune 1st 2014
AHGL Season 4May 31st 2014
Polt - HoTS TutorialsMay 31st 2014
SHOUTcraft Clan WarsMay 26th 2014
DreamHack Open: BucharestApril 28th 2014
GSL Global ChampionshipApril 26th 2014
DreamHack Open: Stockholm 2013April 26th 2014
WCS AM Season 1 2014April 13th 2014
WCS EU Season 1 2014April 13th 2014
Starbow Ladder S1 Cup 3April 12th 2014
GSL Season 1 2014April 5th 2014
GSL Season 1 (Twitch) 2014April 5th 2014
Starbow Ladder S1 Cup 2March 29th 2014
Vasacast InvitationalMarch 23rd 2014
IEM VIII - World ChampionshipMarch 16th 2014
Improve with Apollo - HoTSMarch 3rd 2014
IEM Season VIII - CologneFebruary 16th 2014
ESL Starbow Cup 8February 4th 2014
IEM Season VIII - Sao PauloFebruary 1st 2014
ASUS ROG WinterFebruary 1st 2014
Olimoleys Starbow TournamentJanuary 27th 2014
OGaming Nation WarsJanuary 19th 2014
Husky Starcraft - HoTSDecember 31st 2013
Madals - HoTS Casts 2013December 31st 2013
KhaldorTV HoTS 2013December 31st 2013
DUCKVILLELOL HoTS 2013December 31st 2013
Unfiltered 2013December 30th 2013
BaseTradeTV HoTS Series 2013December 30th 2013
State of the GameDecember 22nd 2013
Numericable M-House Cup 3December 22nd 2013
Bitcoin StarCraft ChallengeDecember 21st 2013
Day[9] Daily 2013December 19th 2013
SC2 Up and Coming 2013December 17th 2013
EPS Germany Winter 2013December 15th 2013
Real TalkDecember 15th 2013
Acer TeamStory Cup S2December 15th 2013
Kaspersky Arena DecemberDecember 11th 2013
Fragbite MastersDecember 9th 2013
Hot6ix Cup 2013December 8th 2013
SHOUTcraft America WinterDecember 8th 2013
ASUS ROG NorthConDecember 7th 2013
IEM Season VIII - SingaporeDecember 1st 2013
World Cyber GamesDecember 1st 2013
DreamHack Open: Winter 2013November 30th 2013
Live on Three 2013November 27th 2013
Red Bull Battlegrounds: NYCNovember 24th 2013
GSTL Season 2November 23rd 2013
Dailymotion CupNovember 23rd 2013
GSTL Season 2 (Mobile)November 23rd 2013
Climbing the LadderNovember 20th 2013
HomeStory Cup VIIINovember 17th 2013
Inside The Game 2013November 12th 2013
WCS Grand FinalsNovember 9th 2013
WCS AM Season 3 2013November 3rd 2013
Meta 2013November 1st 2013
WCS Season 3 FinalsOctober 27th 2013
WCS EU Season 3 2013October 24th 2013
WCS Korea S3 GSL (Mobile)October 19th 2013
WCS Korea Season 3 GSLOctober 19th 2013
Starcraft 2 Survivors: S1October 14th 2013
IEM Season VIII - NYOctober 13th 2013
DreamHack Open: Bucharest 2013September 15th 2013
D-LINK SC2LSeptember 14th 2013
WCS AM Season 2 2013September 5th 2013
Polt - HoTS Tutorials 2013September 3rd 2013
WCS Season 2 FinalsAugust 25th 2013
WCS EU Season 2 2013August 11th 2013
PrOmise - HoTS Casts 2013August 11th 2013
WCS Korea Season 2 OSLAugust 10th 2013
WCS Korea S2 OSL (Mobile)August 10th 2013
Proleague Season 1 (KR/US)August 3rd 2013
ASUS ROG SummerAugust 3rd 2013
Acer TeamStory CupJuly 31st 2013
IEM Season VIII ShanghaiJuly 28th 2013
MLG Summer InviteJuly 28th 2013
Red Bull Training Grounds 2July 27th 2013
DreamHack Open: Valencia 2013July 20th 2013
Rules of EngagementJuly 20th 2013
GSTL Season 1July 20th 2013
GSTL Season 1 (Mobile)July 20th 2013
Esports HeavenJuly 17th 2013
MLG Spring ChampionshipJune 30th 2013
Red Bull Training GroundsJune 23rd 2013
Homestory Cup VIIJune 23rd 2013
WCS AM Season 1 2013June 21st 2013
The GD Show 2013June 20th 2013
DreamHack Open: Summer 2013June 17th 2013
Yegalisk TV - HoTSJune 10th 2013
WCS Season 1 FinalsJune 9th 2013
WCS S1 Finals (Mobile)June 9th 2013
WCS EU Season 1 2013June 5th 2013
WCS Korea Season 1 GSLJune 1st 2013
WCS Korea S1 GSL (Mobile)June 1st 2013
SHOUTcraft AmericaMay 21st 2013
Pro CornerMay 20th 2013
ESET UK Masters Season 1May 13th 2013
Starcraft II - TutorialsMay 10th 2013
AHGL Season 3April 30th 2013
CSL March MadnessApril 25th 2013
The PulseApril 21st 2013
Team Liquid Attack!April 17th 2013
MLG Winter ChampionshipApril 16th 2013
Improve with Apollo - HoTS 2013April 12th 2013
SC2Links.com - HoTS CastsApril 3rd 2013
King of the BetaMarch 19th 2013
GSL Season 1 2013March 9th 2013
IEM World ChampionshipMarch 9th 2013
IPL Fight ClubMarch 8th 2013
GD Studio HoTS KotH SeriesFebruary 28th 2013
IPTL Season 1February 27th 2013
TotalBiscuit ShoutCraft HoTSFebruary 26th 2013
NASL Caster BashFebruary 25th 2013
IPL 6February 20th 2013
IPL DICE ShowdownFebruary 7th 2013
MLG HoTS ExhibitionsFebruary 1st 2013
Iron Squid Chapter II January 26th 2013
IEM KatowiceJanuary 20th 2013
HyperX 10-Year Tournament January 9th 2013
Climbing the Ladder December 27th 2012
HomeStory Cup VI December 22nd 2012
GSL Blizzard CupDecember 22nd 2012
MLG Tournament of Champions December 20th 2012
Live on Three 2012December 19th 2012
Pro Corner December 19th 2012
State of the Game 2012December 18th 2012
IPL Fight Club December 17th 2012
EG Masters Cup Season 8 December 16th 2012
SHOUTCraft HoTS Invitational December 16th 2012
GSL Season 5 December 14th 2012
Day[9] Daily 2012December 13th 2012
NASL Season 4 December 9th 2012
GSTL Season 3December 8th 2012
Improve with ApolloDecember 7th 2012
The GD Show December 4th 2012
GSL World ChampionshipDecember 2nd 2012
IPL 5 Las Vegas December 2nd 2012
GSL World Championship 2011December 1st 2012
IEM Singapore November 24th 2012
DreamHack EIZO Open Winter November 24th 2012
Battle.net World Championship November 18th 2012
Team Liquid Attack! November 14th 2012
IPTL Season 1 November 14th 2012
EG Stormville Series November 12th 2012
GOMTV G-Star Invitational November 12th 2012
Lone Star Clash 2 November 11th 2012
MLG Fall Championship November 4th 2012
MLG MvP November 2nd 2012
BaseTradeTV Showmatch SeriesOctober 31st 2012
OSL Season 1 October 27th 2012
GSL Baneling October 24th 2012
Inside The Game 2012October 23rd 2012
Real Talk 2012October 23rd 2012
DreamHack Open Bucharest October 21st 2012
GSL Season 4 October 20th 2012
WCS Asia Finals October 14th 2012
ASUS ROG The GD Invitational October 14th 2012
Unburrowed October 9th 2012
Shot By Shot September 26th 2012
DreamHack Open Valencia September 23rd 2012
WCS EU Finals September 16th 2012
WCG Korea September 15th 2012
Team Liquid Starleague 4 September 8th 2012
GamersAbyss $100 Showmatch SeriesSeptember 8th 2012
NASL The Gauntlet August 29th 2012
MLG Summer ChampionshipAugust 26th 2012
WCS Korea August 26th 2012
WCS NA Finals August 25th 2012
FXO Invitational Series 6August 19th 2012
IEM Season VII - CologneAugust 18th 2012
WCS Oceania August 12th 2012
ASUS ROG Summer August 4th 2012
IPL Proleague Team Arena Season 3 August 2nd 2012
GSTL Season 2 July 28th 2012
GSL Season 3 2012July 27th 2012
MLG Summer Arena July 22nd 2012
WCS Ukraine July 21st 2012
WCS South America July 16th 2012
NASL Season 3July 15th 2012
GIGABYTE/NVIDIA Invitational 1 July 14th 2012
WCS France July 8th 2012
HomeStory Cup V July 8th 2012
WCS UKJuly 1st 2012
SC2LINKS.com OPEN IIIJune 30th 2012
GESL InvitationalJune 25th 2012
DreamHack EIZO Open: SummerJune 19th 2012
Lone Star ClashJune 17th 2012
WCS USAJune 10th 2012
MLG Spring Championship 2012June 10th 2012
Off The Record June 8th 2012
Red Bull Battlegrounds Austin May 27th 2012
MLG Spring Arena 2May 20th 2012
GSL Season 2 2012May 19th 2012
NASL Sunday ShowdownMay 13th 2012
Iron Squid ParisMay 5th 2012
MLG Spring Arena 1 April 29th 2012
AHGL Season 2April 23rd 2012
DreamHack Open StockholmApril 22nd 2012
GSTL Season 1 2012April 8th 2012
IPL 4 Las VegasApril 8th 2012
MLG Winter Championship 2012April 2nd 2012
NASTL Season 1March 21st 2012
IPL Proleague Team Arena Season 2March 12th 2012
MLG Winter ArenaMarch 5th 2012
GSL Season 1 2012March 3rd 2012
ASUS ROG Assembly WinterFebruary 25th 2012
IEM KievJanuary 22nd 2012
SHOUTcraft Invitational 4January 15th 2012
GSL Blizzard Cup 2011December 17th 2011
NASL Season 2December 8th 2011
GSL NovemberDecember 7th 2011
Dreamhack WinterNovember 26th 2011
MLG ProvidenceNovember 20th 2011
GSL Team Ace Invitational November 13th 2011
MLG Global Invitational November 10th 2011
GSL OctoberOctober 22nd 2011
MLG OrlandoOctober 16th 2011
GSTL Season 1 2011October 8th 2011
GSL Arena of LegendsOctober 2nd 2011
DreamHack Valencia 2011September 17th 2011
GSL AugustSeptember 10th 2011
MLG RaleighAugust 28th 2011
AHGL Season 1August 19th 2011
MLG Anaheim 2011July 31st 2011
GSL JulyJuly 30th 2011
NASL Season 1July 11th 2011
DreamHack SummerJune 21st 2011
GSL Super Tournament 2011June 18th 2011
GSTL MayMay 19th 2011
Team Liquid Starleague 3May 15th 2011
GSL MayMay 14th 2011
GSTL MarchMarch 24th 2011
GSL MarchMarch 19th 2011
GSTL FebruaryFebruary 10th 2011
GSL JanuaryJanuary 29th 2011
GSL Open Season 3December 18th 2010
GSL Open Season 2November 13th 2010
"GSL Open Season 1October 2nd 2010
*/
