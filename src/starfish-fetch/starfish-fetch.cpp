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
#include <QDir>
#include <QDomDocument>
#include <QTextStream>
#include <QTranslator>
#include <stdio.h>

#include "Sc2LinksDotCom.h"
#include "Sc2CastsDotCom.h"
#include "ScClassifier.h"

static ScVodScraper* scraperPtr;
static void showProgress();
static void statusChanged();
static ScVodScraper* makeScraper(const QString& name);
static int year;
static QFile outputFile;


int main(int argc, char** argv) {
    QCoreApplication app(argc, argv);
    QCoreApplication::setApplicationName("starfish-fetch");
    QCoreApplication::setApplicationVersion(QString::asprintf("%d.%d.%d", STARFISH_VERSION_MAJOR, STARFISH_VERSION_MINOR, STARFISH_VERSION_PATCH));

    QTranslator translator;
    if (translator.load("starfish-fetch")) {
        app.installTranslator(&translator);
    }

    QCommandLineOption yearOption(
                QStringList() << "y" << "year",
                "<year> to download",
                "year",
                QString::number(QDateTime::currentDateTime().date().year()));

    QCommandLineOption outputDirectoryOption(
                QStringList() << "d" << "output-directory",
                "Directory to store file in",
                "directory");

    QCommandLineOption scraperOption(
                QStringList() << "s" << "scaper",
                "Name of the <scraper> to use",
                "scraper",
                "sc2casts");

    QCommandLineOption classifierFilePathOption(
                QStringList() << "c" << "classifier",
                "Path to classifier definition file",
                "classifier");

    QCommandLineOption scrapersOption(
                QStringList() << "scrapers",
                "Print list of available scrapers");

    QCommandLineParser parser;
    parser.setApplicationDescription("Copyright (c) 2018, 2019 Jean Gressmann\nstarfish-fetch fetches VOD data from the internet and outputs it as xml");
    parser.addHelpOption();
    parser.addVersionOption();
    parser.addOption(yearOption);
    parser.addOption(outputDirectoryOption);
    parser.addOption(scraperOption);
    parser.addOption(scrapersOption);
    parser.addOption(classifierFilePathOption);

    parser.process(app);

    if (parser.isSet(scrapersOption)) {
        fprintf(stdout, "Available scrapers:\n");
        fprintf(stdout, "sc2casts\n");
        fprintf(stdout, "sc2links\n");
        return 0;
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

    auto classifierFilePath = parser.value(classifierFilePathOption);
    ScClassifier classifier;
    if (!classifier.load(classifierFilePath)) {
        fprintf(stderr, "Failed to load classifier from %s\n", qPrintable(classifierFilePath));
        return -1;
    }

    auto outputDirectory = parser.value(outputDirectoryOption);
    if (outputDirectory.isEmpty()) {
        outputFile.open(stdout, QIODevice::WriteOnly);
    } else {
        QDir dir;
        if (!dir.mkpath(outputDirectory)) {
            fprintf(stderr, "Failed to create directory %s\n", qPrintable(outputDirectory));
            return -1;
        }

        dir.setPath(outputDirectory);
        auto filePath = dir.filePath(QString::number(year) + QStringLiteral(".xml"));
        outputFile.setFileName(filePath);
        outputFile.open(QIODevice::WriteOnly | QIODevice::Truncate);
    }

    if (!outputFile.isOpen()) {
        fprintf(stderr, "Failed to open %s for writing\n", qPrintable(outputFile.fileName()));
        return -1;
    }



    QScopedPointer<ScVodScraper> scraper(makeScraper(parser.value(scraperOption)));
    if (!scraper) {
        fprintf(stderr, "Unknown scraper '%s'\n", qPrintable(parser.value(scraperOption)));
        return -1;
    }

    scraper->setClassifier(&classifier);
    scraper->setYearToFetch(year);

//    ScVodScraper::connect(scraper.data(), &ScVodScraper::excludeEvent, excludeEvent);
    ScVodScraper::connect(scraper.data(), &ScVodScraper::statusChanged, statusChanged);
    ScVodScraper::connect(scraper.data(), &ScVodScraper::progressChanged, showProgress);
    ScVodScraper::connect(scraper.data(), &ScVodScraper::progressDescriptionChanged, showProgress);

    scraperPtr = scraper.data();


    // start download
    scraper->startFetch();

    return app.exec();
}


static void writeXml()
{
    QDomDocument doc;

    QDomElement vods = doc.createElement(QStringLiteral("vods"));
    vods.setAttribute(QStringLiteral("version"), 1);
    vods.setAttribute(QStringLiteral("created"), QDateTime::currentDateTime().toString(Qt::ISODate));
    QDomElement vodsElement = doc.createElement(QStringLiteral("records"));
    foreach (const auto& record, scraperPtr->vods()) {
        auto node = record.toXml(doc);
        vodsElement.appendChild(node);
    }

    vods.appendChild(vodsElement);

    doc.appendChild(doc.createProcessingInstruction(QStringLiteral("xml"), QStringLiteral("version=\"1.0\" encoding=\"utf-8\"")));
    doc.appendChild(vods);

    QTextStream s(&outputFile);
    s.setCodec("UTF-8");
    doc.save(s, 1);
}

static void statusChanged()
{
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

static ScVodScraper* makeScraper(const QString& name)
{
    if ("sc2casts" == name.toLower()) {
        return new Sc2CastsDotCom();
    }

    if ("sc2links" == name.toLower()) {
        return new Sc2LinksDotCom();
    }

    return Q_NULLPTR;
}

static void showProgress()
{
    fprintf(stdout, "%02d%% %s\n", static_cast<int>(scraperPtr->progress() * 100), qPrintable(scraperPtr->progressDescription()));
}

