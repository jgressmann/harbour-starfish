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

#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QDateTime>
#include <QFile>
#include <QDomDocument>
#include <QTextStream>
#include <stdio.h>

#include "Sc2LinksDotCom.h"
#include "Sc2CastsDotCom.h"

static ScVodScraper* scraperPtr;
//static void excludeEvent(const ScEvent& event, bool* exclude);
static void excludeRecord(const ScRecord& record, bool* exclude);
static void statusChanged();
static ScVodScraper* makeScraper(const QString& name);
static int year;
static QFile outputFile;

int main(int argc, char** argv) {
    QCoreApplication app(argc, argv);
    QCoreApplication::setApplicationName("starfish-fetch");
    QCoreApplication::setApplicationVersion(QString::asprintf("%d.%d.%d", STARFISH_VERSION_MAJOR, STARFISH_VERSION_MINOR, STARFISH_VERSION_PATCH));

    QCommandLineOption yearOption(
                QStringList() << "y" << "year",
                "<year> to download",
                "year",
                QString::number(QDateTime::currentDateTime().date().year()));

    QCommandLineOption fileOption(
                QStringList() << "o" << "output-file",
                "Name of the <file> to save the data in",
                "file");

    QCommandLineOption scraperOption(
                QStringList() << "s" << "scaper",
                "Name of the <scaper> to use",
                "scraper",
                "sc2casts");

    QCommandLineOption scrapersOption(
                QStringList() << "scapers",
                "Print list of available scrapers");

    QCommandLineParser parser;
    parser.setApplicationDescription("Copyright (c) 2018 Jean Gressmann\nstarfish-fetch fetches vod data from the internet and outputs it as xml");
    parser.addHelpOption();
    parser.addVersionOption();
    parser.addOption(yearOption);
    parser.addOption(fileOption);
    parser.addOption(scraperOption);
    parser.addOption(scrapersOption);

    parser.process(app);

    if (parser.isSet(scrapersOption)) {
        fprintf(stdout, "Available scrapers:\n");
        fprintf(stdout, "sc2casts\n");
        fprintf(stdout, "sc2links\n");
        return 0;
    }

    auto outputFilePath = parser.value(fileOption);
    if (outputFilePath.isEmpty()) {
        outputFile.open(stdout, QIODevice::WriteOnly);
        outputFilePath = "stdout";
    } else {
        outputFile.setFileName(outputFilePath);
        outputFile.open(QIODevice::WriteOnly | QIODevice::Truncate);
    }

    if (!outputFile.isOpen()) {
        fprintf(stderr, "Failed to open %s for writing\n", qPrintable(outputFilePath));
        return -1;
    }

    {
        bool yearOk = false;
        auto yearValue = parser.value(yearOption);
        year = yearValue.toInt(&yearOk);
        if (!yearOk) {
            fprintf(stderr, "Could not convert '%s' to integer\n", qPrintable(yearValue));
            return -1;
        }
    }

    QScopedPointer<ScVodScraper> scraper(makeScraper(parser.value(scraperOption)));
    if (!scraper) {
        fprintf(stderr, "Unknown scraper '%s'\n", qPrintable(parser.value(scraperOption)));
        return -1;
    }

//    ScVodScraper::connect(scraper.data(), &ScVodScraper::excludeEvent, excludeEvent);
    ScVodScraper::connect(scraper.data(), &ScVodScraper::excludeRecord, excludeRecord);
    ScVodScraper::connect(scraper.data(), &ScVodScraper::statusChanged, statusChanged);
    void excludeRecord(const ScRecord& record, bool* exclude);
    scraperPtr = scraper.data();


    // start download
    scraper->startFetch();

    return app.exec();
}

//static void excludeEvent(const ScEvent& event, bool* exclude) {
//    Q_ASSERT(exclude);
//    *exclude = event.year() != year;
//}

static void excludeRecord(const ScRecord& record, bool* exclude) {
    Q_ASSERT(exclude);
    if (record.valid & ScRecord::ValidYear) {
        *exclude = record.year != year;
        if (record.year < year) {
            scraperPtr->cancelFetch();
        }
    }
}

static void writeXml() {
    QDomDocument doc;

    QDomElement vods = doc.createElement(QStringLiteral("vods"));
    vods.setAttribute(QStringLiteral("version"), 1);
    vods.setAttribute(QStringLiteral("created"), QDateTime::currentDateTime().toString(Qt::ISODate));
    QDomElement vodsElement = doc.createElement(QStringLiteral("records"));
    foreach (const auto& record, scraperPtr->vods()) {
        auto node = record.toXml(doc);
        vodsElement.appendChild(node);
    }

//    QDomElement seriesElement = doc.createElement(QStringLiteral("series"));

//    foreach (const auto& serie, sitePtr->vods()) {
//        QDomElement serieElement = doc.createElement("serie");
//        serieElement.setAttribute(QStringLiteral("name"), serie.name());
//        serieElement.setAttribute(QStringLiteral("url"), serie.url());

//        QDomElement eventsElement = doc.createElement(QStringLiteral("events"));

//        foreach (const auto& event, serie.events()) {
//            QDomElement eventElement = doc.createElement(QStringLiteral("event"));
//            eventElement.setAttribute(QStringLiteral("name"), event.name());
//            eventElement.setAttribute(QStringLiteral("url"), event.url());
//            eventElement.setAttribute(QStringLiteral("fullName"), event.fullName());
//            eventElement.setAttribute(QStringLiteral("year"), event.year());
//            eventElement.setAttribute(QStringLiteral("season"), event.season());
//            eventElement.setAttribute(QStringLiteral("game"), event.game() == ScEnums::Game_Broodwar ? QStringLiteral("Broodwar") : QStringLiteral("StarCraft II"));

//            QDomElement stagesElement = doc.createElement(QStringLiteral("stages"));
//            foreach (const auto& stage, event.stages()) {
//                QDomElement stageElement = doc.createElement(QStringLiteral("stage"));
//                stageElement.setAttribute(QStringLiteral("name"), stage.name());
//                stageElement.setAttribute(QStringLiteral("url"), stage.url());

//                QDomElement matchesElement = doc.createElement(QStringLiteral("matches"));
//                foreach (const auto& match, stage.matches()) {
//                    QDomElement matchElement = doc.createElement(QStringLiteral("match"));
//                    matchElement.setAttribute(QStringLiteral("name"), match.name());
//                    matchElement.setAttribute(QStringLiteral("url"), match.url());
//                    matchElement.setAttribute(QStringLiteral("date"), match.matchDate().toString(QStringLiteral("yyyy-MM-dd")));
//                    matchElement.setAttribute(QStringLiteral("side1"), match.side1());
//                    matchElement.setAttribute(QStringLiteral("side2"), match.side2());
//                    matchElement.setAttribute(QStringLiteral("side2"), match.side2());
//                    matchElement.setAttribute(QStringLiteral("number"), match.matchNumber());

//                    matchesElement.appendChild(matchElement);
//                }

//                stageElement.appendChild(matchesElement);
//                stagesElement.appendChild(stageElement);
//            }

//            eventElement.appendChild(stagesElement);
//            eventsElement.appendChild(eventElement);
//        }

//        serieElement.appendChild(eventsElement);
//        seriesElement.appendChild(serieElement);
//    }

//    vods.appendChild(seriesElement);

    vods.appendChild(vodsElement);

    doc.appendChild(doc.createProcessingInstruction(QStringLiteral("xml"), QStringLiteral("version=\"1.0\" encoding=\"utf-8\"")));
    doc.appendChild(vods);

    QTextStream s(&outputFile);
    s.setCodec("UTF-8");
    doc.save(s, 1);
}

static void statusChanged() {
    Q_ASSERT(scraperPtr);

    switch (scraperPtr->status()) {
    case Sc2LinksDotCom::Status_VodFetchingComplete:
    case Sc2LinksDotCom::Status_VodFetchingCanceled:
        writeXml();
        qApp->exit();
        break;
    case Sc2LinksDotCom::Status_Error:
        switch (scraperPtr->error()) {
        case Sc2LinksDotCom::Error_NetworkFailure:
            qApp->exit(-2);
            break;
        default:
            qApp->exit(-3);
            break;
        }
        break;
    default:
        break;
    }
}

static ScVodScraper* makeScraper(const QString& name) {
    if ("sc2casts" == name.toLower()) {
        return new Sc2CastsDotCom();
    }

    if ("sc2links" == name.toLower()) {
        return new Sc2LinksDotCom();
    }

    return Q_NULLPTR;
}
