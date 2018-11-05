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

#include "ScVodDataManager.h"
#include "ScVodDataManagerState.h"
#include "ScVodDataManagerWorker.h"

#include "Vods.h"
#include "Sc.h"
#include "ScRecord.h"
#include "ScStopwatch.h"
#include "ScApp.h"



#include <QDebug>
#include <QRegExp>
#include <QMutexLocker>
#include <QSqlQuery>
#include <QSqlError>
#include <QSqlRecord>
#include <QFile>
#include <QDir>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QTemporaryFile>
#include <QUrl>
#include <QUrlQuery>
#include <QMimeDatabase>
#include <QMimeData>
#include <stdio.h>
#include <cassert>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>


#ifndef _countof
#   define _countof(x) (sizeof(x)/sizeof((x)[0]))
#endif

#define RETURN_IF_ERROR \
    do { \
        if (m_State != State_Ready) { \
            qCritical("Not ready, return"); \
        } \
    } while (0)

#define RETURN_MINUS_ON_ERROR \
    do { \
        if (m_State != State_Ready) { \
            qCritical("Not ready, return"); \
            return -1; \
        } \
    } while (0)

#define CLASSIFIER_FILE "/classifier.json.gz"
#define ICONS_FILE "/icons.json.gz"
#define SQL_PATCH_FILE "/sql_patches.json.gz"
#define DATA_DIR_KEY "data-dir"
namespace  {

const QString s_IconsUrl = QStringLiteral("https://www.dropbox.com/s/p2640l82i3u1zkg/icons.json.gz?dl=1");
const QString s_ClassifierUrl = QStringLiteral("https://www.dropbox.com/s/3cckgyzlba8kev9/classifier.json.gz?dl=1");
const QString s_SqlPatchesUrl = QStringLiteral("https://www.dropbox.com/s/sip5esgcba6hwfo/sql_patches.json.gz?dl=1");
const QString s_SqlPatchLevelKey = QStringLiteral("sql_patch_level");
const QString s_SqlPatchLevelDefault = QStringLiteral("0");
const QString s_ProtossIconPath = QStringLiteral(QT_STRINGIFY(SAILFISH_DATADIR) "/media/protoss.png");
const QString s_TerranIconPath = QStringLiteral(QT_STRINGIFY(SAILFISH_DATADIR) "/media/terran.png");
const QString s_ZergIconPath = QStringLiteral(QT_STRINGIFY(SAILFISH_DATADIR) "/media/zerg.png");
const QString s_BroodWarIconPath = QStringLiteral(QT_STRINGIFY(SAILFISH_DATADIR) "/media/bw.png");
const QString s_Sc2IconPath = QStringLiteral(QT_STRINGIFY(SAILFISH_DATADIR) "/media/sc2.png");
const QString s_RandomIconPath = QStringLiteral(QT_STRINGIFY(SAILFISH_DATADIR) "/media/random.png");
const QString s_SailfishIconPath = QStringLiteral("image://theme/icon-m-sailfish");
const QString s_GameKey = QStringLiteral("game");
const QString s_EventNameKey = QStringLiteral("event_name");

const int BatchSize = 1<<13;




} // anon


ScWorker::ScWorker(ScThreadFunction f, void* arg)
    : m_Function(f)
    , m_Arg(arg) {
}


void ScWorker::process() {
    m_Function(m_Arg);
    emit finished();
}

////////////////////////////////////////////////////////////////////////////////
void DataDirectoryMover::move(const QString& currentDir, const QString& targetDir) {
    _move(currentDir, targetDir);
    deleteLater();
}

void DataDirectoryMover::_move(const QString& currentDir, QString targetDir) {
    qDebug() << "move from" << currentDir << "to" << targetDir;
    QDir d(targetDir);
    if (!d.exists()) {
        if (!d.mkpath(d.absolutePath())) {
            qWarning() << "failed to create" << targetDir;
            emit dataDirectoryChanging(ScVodDataManager::DDCT_Finished, targetDir, 0, ScVodDataManager::Error_ProcessOutOfResources, QStringLiteral("Failed to create target directory").arg(targetDir));
            return;
        }
    }

    targetDir = d.absolutePath();

    dev_t device1, device2;
    struct stat sb;
    if (-1 == lstat(currentDir.toLocal8Bit().data(), &sb)) {
        emit dataDirectoryChanging(
                    ScVodDataManager::DDCT_Finished,
                    currentDir,
                    0,
                    ScVodDataManager::Error_StatFailed,
                    QStringLiteral("Failed to stat %1: %2 (%3)").arg(currentDir, QString::number(errno), QString::fromLocal8Bit(strerror(errno))));
    }

    device1 = sb.st_dev;

    if (-1 == lstat(targetDir.toLocal8Bit().data(), &sb)) {
        emit dataDirectoryChanging(
                    ScVodDataManager::DDCT_Finished,
                    targetDir,
                    0,
                    ScVodDataManager::Error_StatFailed,
                    QStringLiteral("Failed to stat %1: %2 (%3)").arg(targetDir, QString::number(errno), QString::fromLocal8Bit(strerror(errno))));
    }

    device2 = sb.st_dev;

    const QString names[] = {
        QStringLiteral("/meta/"),
        QStringLiteral("/thumbnails/"),
        QStringLiteral("/vods/"),
        QStringLiteral("/icons/"),
    };

    if (device1 == device2) {
        qDebug() << "directories are on the same device, using rename";
        // move dirs
        for (size_t i = 0; i < _countof(names); ++i) {
            auto src = currentDir + names[i];
            auto dst = targetDir + names[i];
            if (!d.rename(src, dst)) {
                // ouch
                emit dataDirectoryChanging(
                            ScVodDataManager::DDCT_Finished,
                            src,
                            0.25f * i,
                            ScVodDataManager::Error_RenameFailed,
                            QStringLiteral("Failed to move %1 to %2").arg(src, dst));
                return;
            }

            emit dataDirectoryChanging(
                        ScVodDataManager::DDCT_Move,
                        src,
                        0.25f * i,
                        0,
                        QStringLiteral("Moved %1 to %2").arg(src, dst));
        }


    } else {
        qDebug() << "directories are on diffrent devices, using rsync";
        // rsync dirs
        QProcess process;
//        connect(&process, SIGNAL(finished(int, QProcess::ExitStatus)),
//                this, SLOT(onProcessFinished(int, QProcess::ExitStatus)));
//        connect(&process, SIGNAL(error(QProcess::ProcessError)),
//                this, SLOT(onProcessError(QProcess::ProcessError)));
        connect(&process, &QProcess::readyReadStandardOutput,
                this, &DataDirectoryMover::onProcessReadyRead);


        // copy dirs to new fs
        QStringList args;
        for (size_t i = 0; i < _countof(names); ++i) {
            auto src = currentDir + names[i];
            src.chop(1); // remove trailing slash

            args.clear();
            args << QStringLiteral("-av") << src << targetDir; // use -v to have a frequent callback to enable cancel
            process.start(QStringLiteral("rsync"), args, QIODevice::ReadOnly);
            process.waitForFinished(-1);

            if (m_Canceled != 0) {
                emit dataDirectoryChanging(
                            ScVodDataManager::DDCT_Finished,
                            QString(),
                            0.25f * i,
                            ScVodDataManager::Error_Canceled,
                            QString());
                return;
            }

            auto exitCode = process.exitCode();
            auto err = process.readAllStandardError();

//            auto out = process.readAllStandardOutput();
            qDebug() << "rsync exited with code" << exitCode;
//            qDebug() << "rsync stdout";
//            qDebug() << out;
            qDebug() << "rsync stderr";
            qDebug() << err;
            switch (exitCode) {
            case 0:
                break;
            case 11: // error in file i/o
                err.push_back('\0');
                if (strstr(err.data(), "(28)")) {
                    // no space left on device
                    emit dataDirectoryChanging(
                                ScVodDataManager::DDCT_Finished,
                                src,
                                0.25f * i,
                                ScVodDataManager::Error_NoSpaceLeftOnDevice,
                                QStringLiteral("no space left on device"));
                } else {
                    err.chop(1);
                    emit dataDirectoryChanging(
                                ScVodDataManager::DDCT_Finished,
                                src,
                                0.25f * i,
                                ScVodDataManager::Error_ProcessFailed,
                                QStringLiteral("'rsync %1' failed: %2").arg(args.join(" "), QString::fromLocal8Bit(err)));
                }
                return;
            default: {
                emit dataDirectoryChanging(
                            ScVodDataManager::DDCT_Finished,
                            src,
                            0.25f * i,
                            ScVodDataManager::Error_ProcessFailed,
                            QStringLiteral("'rsync %1' failed: %2").arg(args.join(" "), QString::fromLocal8Bit(err)));
                return;
            } break;
            }

            emit dataDirectoryChanging(
                        ScVodDataManager::DDCT_Copy,
                        src,
                        0.25f * i,
                        0,
                        QStringLiteral("Copied %1 to %2").arg(src, targetDir + names[i]));
        }


        // remove source dirs
        for (size_t i = 0; i < _countof(names); ++i) {
            auto src = currentDir + names[i];
            if (!QDir(src).removeRecursively()) {
                emit dataDirectoryChanging(
                            ScVodDataManager::DDCT_Remove,
                            src,
                            1,
                            ScVodDataManager::Error_Warning,
                            QStringLiteral("Failed to remove directory %1").arg(src));
            }

            emit dataDirectoryChanging(
                        ScVodDataManager::DDCT_Remove,
                        src,
                        1,
                        0,
                        QStringLiteral("Removed directory %1").arg(src));
        }
    }

    emit dataDirectoryChanging(
                ScVodDataManager::DDCT_Finished,
                targetDir,
                1,
                ScVodDataManager::Error_None,
                QString());
}

void DataDirectoryMover::cancel() {
    m_Canceled = 1;
}

void DataDirectoryMover::onProcessReadyRead() {
    QProcess* process = qobject_cast<QProcess*>(QObject::sender());
    Q_ASSERT(process);

    if (0 != m_Canceled) {
        process->kill();
    }

    // erase stdout
    process->readAllStandardOutput();
}

////////////////////////////////////////////////////////////////////////////////




ScVodDataManager::~ScVodDataManager() {
    qDebug() << "dtor entry";

    setState(State_Finalizing);
    qDebug() << "set finalizing";

    cancelMoveDataDirectory();

    emit stopWorker();
    m_WorkerThread.quit();
    m_WorkerThread.wait();
    qDebug() << "stopped worker thread";

    if (m_ClassfierRequest) {
        m_ClassfierRequest->abort();
        delete m_ClassfierRequest;
        qDebug() << "aborted classifier request";
    }


    if (m_SqlPatchesRequest) {
        m_SqlPatchesRequest->abort();
        delete m_SqlPatchesRequest;
        qDebug() << "aborted sql patches request";
    }

    foreach (auto key, m_IconRequests.keys()) {
        key->abort();
        delete key;
    }
    qDebug() << "aborted icons requests";

    m_AddThread.quit();
    m_AddThread.wait();

    delete m_Manager;
    m_Manager = nullptr;
    qDebug() << "deleted network manager";


    m_Database = QSqlDatabase(); // to remove ref count
    QSqlDatabase::removeDatabase(QStringLiteral("ScVodDataManager"));

    qDebug() << "dtor exit";
}

ScVodDataManager::ScVodDataManager(QObject *parent)
    : QObject(parent)
    , m_Lock(QMutex::Recursive)
    , m_AddQueueLock(QMutex::NonRecursive)
    , m_Manager(nullptr)
    , m_WorkerInterface(nullptr)
    , m_ClassfierRequest(nullptr)
    , m_SqlPatchesRequest(nullptr)
    , m_DataDirectoryMover(nullptr)
    , m_State(State_Initializing)
    , m_SharedState(new ScVodDataManagerState)
{
    m_Manager = new QNetworkAccessManager(this);
    connect(m_Manager, &QNetworkAccessManager::finished, this,
            &ScVodDataManager::requestFinished);

    m_Error = Error_None;
    m_SuspendedVodsChangedEventCount = 0;
    m_MaxConcurrentMetaDataDownloads = 4;

    const auto databaseDir = QStandardPaths::writableLocation(QStandardPaths::DataLocation);

    QDir d;
    d.mkpath(databaseDir);

    m_SharedState->DatabaseFilePath = databaseDir + QStringLiteral("/vods.sqlite");
    m_Database = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), QStringLiteral("ScVodDataManager"));
    m_Database.setDatabaseName(m_SharedState->DatabaseFilePath);
    if (m_Database.open()) {
        qInfo("Opened database %s\n", qPrintable(m_SharedState->DatabaseFilePath));
        setupDatabase();
    } else {
        qCritical("Could not open database '%s'. Error: %s\n", qPrintable(m_SharedState->DatabaseFilePath), qPrintable(m_Database.lastError().text()));
        setError(Error_Unknown);
        return;
    }

    setDirectories();


    d.mkpath(m_SharedState->m_MetaDataDir);
    d.mkpath(m_SharedState->m_ThumbnailDir);
    d.mkpath(m_SharedState->m_VodDir);
    d.mkpath(m_SharedState->m_IconDir);


    // icons
    const auto iconsFileDestinationPath = databaseDir + QStringLiteral(ICONS_FILE);
    if (!QFile::exists(iconsFileDestinationPath)) {
        auto src = QStringLiteral(QT_STRINGIFY(SAILFISH_DATADIR) ICONS_FILE);
        if (!QFile::copy(src, iconsFileDestinationPath)) {
            qCritical() << "could not copy" << src << "to" << iconsFileDestinationPath;
            setError(Error_Unknown);
            return;
        }
    }

    auto result = m_Icons.load(iconsFileDestinationPath);
    if (!result) {
        qCritical() << "failed to load icons data from" << iconsFileDestinationPath;
        setError(Error_Unknown);
        return;
    }

    // classifier
    const auto classifierFileDestinationPath = databaseDir + QStringLiteral(CLASSIFIER_FILE);
    if (!QFile::exists(classifierFileDestinationPath)) {
        auto src = QStringLiteral(QT_STRINGIFY(SAILFISH_DATADIR) CLASSIFIER_FILE);
        if (!QFile::copy(src, classifierFileDestinationPath)) {
            qCritical() << "could not copy" << src << "to" << iconsFileDestinationPath;
            setError(Error_Unknown);
            return;
        }
    }

    result = m_Classifier.load(classifierFileDestinationPath);
    if (!result) {
        qCritical() << "failed to load classifier data from" << classifierFileDestinationPath;
        setError(Error_Unknown);
        return;
    }

    setState(State_Ready);

    fetchIcons();
    fetchClassifier();
    fetchSqlPatches();

    auto adder = new ScWorker(&ScVodDataManager::batchAddVods, this);
    adder->moveToThread(&m_AddThread);
    connect(this, &ScVodDataManager::vodsToAdd, adder, &ScWorker::process);
    connect(adder, &ScWorker::finished, this, &ScVodDataManager::vodAddWorkerFinished);
    connect(&m_AddThread, &QThread::finished, adder, &QObject::deleteLater);
    m_AddThread.start(QThread::LowestPriority);


    m_WorkerInterface = new ScVodDataManagerWorker(m_SharedState);
    m_WorkerInterface->moveToThread(&m_WorkerThread);

    // worker->manager
    connect(m_WorkerInterface, &ScVodDataManagerWorker::fetchingMetaData, this, &ScVodDataManager::fetchingMetaData);
    connect(m_WorkerInterface, &ScVodDataManagerWorker::fetchingThumbnail, this, &ScVodDataManager::fetchingThumbnail);
    connect(m_WorkerInterface, &ScVodDataManagerWorker::metaDataAvailable, this, &ScVodDataManager::metaDataAvailable);
    connect(m_WorkerInterface, &ScVodDataManagerWorker::metaDataDownloadFailed, this, &ScVodDataManager::metaDataDownloadFailed);
    connect(m_WorkerInterface, &ScVodDataManagerWorker::vodAvailable, this, &ScVodDataManager::vodAvailable);
    connect(m_WorkerInterface, &ScVodDataManagerWorker::thumbnailAvailable, this, &ScVodDataManager::thumbnailAvailable);
    connect(m_WorkerInterface, &ScVodDataManagerWorker::thumbnailDownloadFailed, this, &ScVodDataManager::thumbnailDownloadFailed);
    connect(m_WorkerInterface, &ScVodDataManagerWorker::vodDownloadFailed, this, &ScVodDataManager::vodDownloadFailed);
    connect(m_WorkerInterface, &ScVodDataManagerWorker::vodDownloadCanceled, this, &ScVodDataManager::vodDownloadCanceled);
    connect(m_WorkerInterface, &ScVodDataManagerWorker::vodDownloadsChanged, this, &ScVodDataManager::onVodDownloadsChanged);
    connect(m_WorkerInterface, &ScVodDataManagerWorker::titleAvailable, this, &ScVodDataManager::titleAvailable);
    connect(m_WorkerInterface, &ScVodDataManagerWorker::seenAvailable, this, &ScVodDataManager::seenAvailable);
    connect(m_WorkerInterface, &ScVodDataManagerWorker::vodEndAvailable, this, &ScVodDataManager::vodEndAvailable);
    // manager->worker
    connect(this, &ScVodDataManager::startWorker, m_WorkerInterface, &ScVodDataManagerWorker::process);
    connect(this, &ScVodDataManager::stopWorker, m_WorkerInterface, &QObject::deleteLater);
    connect(this, &ScVodDataManager::maxConcurrentMetaDataDownloadsChanged, m_WorkerInterface, &ScVodDataManagerWorker::maxConcurrentMetaDataDownloadsChanged);

    m_WorkerThread.start();
}

void
ScVodDataManager::requestFinished(QNetworkReply* reply) {
#define RETURN_ON_REQUEST \
    do { \
        switch (m_State) { \
        case State_Error: \
        case State_Finalizing: \
            return; \
        } \
    } while (0)

    ScStopwatch sw("requestFinished");
    QMutexLocker guard(&m_Lock);
    reply->deleteLater();

    auto it2 = m_IconRequests.find(reply);
    if (it2 != m_IconRequests.end()) {
        auto r = it2.value();
        m_IconRequests.erase(it2);

        RETURN_ON_REQUEST;

        iconRequestFinished(reply, r);
    } else {
        if (m_ClassfierRequest == reply) {
            m_ClassfierRequest = nullptr;

            RETURN_ON_REQUEST;

            switch (reply->error()) {
            case QNetworkReply::OperationCanceledError:
                break;
            case QNetworkReply::NoError: {
                // Get the http status code
                int v = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
                if (v >= 200 && v < 300) {// Success
                    // Here we got the final reply
                    auto bytes = reply->readAll();
                    QFile file(QStandardPaths::writableLocation(QStandardPaths::DataLocation) + QStringLiteral(CLASSIFIER_FILE));
                    if (file.open(QIODevice::Truncate | QIODevice::WriteOnly)) {
                        auto w = file.write(bytes);
                        file.close();
                        if (-1 == w) {
                            qWarning() << "failed to write" << file.fileName() << "error" << file.errorString();
                        } else if (w < bytes.size()) {
                            qWarning() << "failed to write to file (no space)" << file.fileName() << "error" << file.errorString() << "file will be removed";
                            file.remove();
                        } else {
                            ScClassifier classifier;
                            if (classifier.load(file.fileName())) {
                                m_Classifier = classifier;
                            } else {
                                qCritical() << "Failed to load icons from downloaded icons file" << reply->url();
                            }
                        }
                    } else {
                        qWarning() << "failed to open" << file.fileName() << "for writing";
                    }
                }
                else if (v >= 300 && v < 400) {// Redirection

                    // Get the redirection url
                    QUrl newUrl = reply->attribute(QNetworkRequest::RedirectionTargetAttribute).toUrl();
                    // Because the redirection url can be relative,
                    // we have to use the previous one to resolve it
                    newUrl = reply->url().resolved(newUrl);

                    m_ClassfierRequest = m_Manager->get(Sc::makeRequest(newUrl));
                } else  {
                    qDebug() << "Http status code:" << v;
                }
            } break;
            default: {
                qDebug() << "Network request failed: " << reply->errorString() << reply->url();
            } break;
            }
        } else {
            if (m_SqlPatchesRequest == reply) {
                m_SqlPatchesRequest = nullptr;

                RETURN_ON_REQUEST;

                switch (reply->error()) {
                case QNetworkReply::OperationCanceledError:
                    break;
                case QNetworkReply::NoError: {
                    // Get the http status code
                    int v = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
                    if (v >= 200 && v < 300) {// Success
                        // Here we got the final reply
                        auto bytes = reply->readAll();
                        bytes = Sc::gzipDecompress(bytes);
                        applySqlPatches(bytes);
                    }
                    else if (v >= 300 && v < 400) {// Redirection

                        // Get the redirection url
                        QUrl newUrl = reply->attribute(QNetworkRequest::RedirectionTargetAttribute).toUrl();
                        // Because the redirection url can be relative,
                        // we have to use the previous one to resolve it
                        newUrl = reply->url().resolved(newUrl);

                        m_SqlPatchesRequest = m_Manager->get(Sc::makeRequest(newUrl));
                    } else  {
                        qDebug() << "Http status code:" << v;
                    }
                } break;
                default: {
                    qDebug() << "Network request failed: " << reply->errorString() << reply->url();
                } break;
                }
            }
        }
    }

#undef RETURN_ON_REQUEST
}


void
ScVodDataManager::iconRequestFinished(QNetworkReply* reply, IconRequest& r) {
    switch (reply->error()) {
    case QNetworkReply::OperationCanceledError:
        break;
    case QNetworkReply::NoError: {
        // Get the http status code
        int v = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        if (v >= 200 && v < 300) {// Success
            if (r.url == s_IconsUrl) {
                // Here we got the final reply
                auto bytes = reply->readAll();
                QFile file(QStandardPaths::writableLocation(QStandardPaths::DataLocation) + QStringLiteral(ICONS_FILE));
                if (file.open(QIODevice::Truncate | QIODevice::WriteOnly)) {
                    auto w = file.write(bytes);
                    file.close();
                    if (-1 == w) {
                        qWarning() << "failed to write" << file.fileName() << "error" << file.errorString();
                    } else if (w < bytes.size()) {
                        qWarning() << "failed to write to file (no space)" << file.fileName() << "error" << file.errorString() << "file will be removed";
                        file.remove();
                    } else {
                        ScIcons icons;
                        if (icons.load(file.fileName())) {
                            m_Icons = icons;

                            // create entries and fetch files
                            QSqlQuery q(m_Database);
                            const auto& icons = m_Icons;
                            for (auto i = 0; i < icons.size(); ++i) {
                                auto url = icons.url(i);

                                // insert row for thumbnail
                                if (!q.prepare(
                                            QStringLiteral("SELECT file_name FROM icons WHERE url=?"))) {
                                    qCritical() << "failed to prepare query" << q.lastError();
                                    continue;
                                }

                                q.addBindValue(url);

                                if (!q.exec()) {
                                    qCritical() << "failed to exec query" << q.lastError();
                                    return;
                                }

                                if (q.next()) {
                                    auto filePath = m_SharedState->m_IconDir + q.value(0).toString();
                                    QFileInfo fi(filePath);
                                    if (fi.exists() && fi.size() > 0) {
                                        continue;
                                    }
                                } else {
                                    auto extension = icons.extension(i);
                                    QTemporaryFile tempFile(m_SharedState->m_IconDir + QStringLiteral("XXXXXX") + extension);
                                    tempFile.setAutoRemove(false);
                                    if (tempFile.open()) {
                                        auto iconFilePath = tempFile.fileName();
                                        auto tempFileName = QFileInfo(iconFilePath).fileName();

                                        // insert row for icon
                                        if (!q.prepare(
                                                    QStringLiteral("INSERT INTO icons (url, file_name) VALUES (?, ?)"))) {
                                            qCritical() << "failed to prepare query" << q.lastError();
                                            continue;
                                        }

                                        q.addBindValue(url);
                                        q.addBindValue(tempFileName);

                                        if (!q.exec()) {
                                            qCritical() << "failed to exec query" << q.lastError();
                                            continue;
                                        }
                                        qDebug() << "add icon" << url << tempFileName;
                                    } else {
                                        qWarning() << "failed to allocate temporary file for icon";
                                        continue;
                                    }
                                }

                                auto reply = m_Manager->get(Sc::makeRequest(url));
                                IconRequest r;
                                r.url = url;
                                m_IconRequests.insert(reply, r);
                            }
                        } else {
                            qCritical() << "Failed to load icons from downloaded icons file" << reply->url();
                        }
                    }
                } else {
                    qWarning() << "failed to open" << file.fileName() << "for writing";
                }
            } else {
                QSqlQuery q(m_Database);
                if (!q.prepare(
                            QStringLiteral("SELECT file_name FROM icons WHERE url=?"))) {
                    qCritical() << "failed to prepare query" << q.lastError();
                    return;
                }

                q.addBindValue(r.url);

                if (!q.exec()) {
                    qCritical() << "failed to exec query" << q.lastError();
                    return;
                }

                if (!q.next()) {
                    return; // cleared?
                }

                auto iconFilePath = m_SharedState->m_IconDir + q.value(0).toString();

                QFile file(iconFilePath);
                if (file.open(QIODevice::Truncate | QIODevice::WriteOnly)) {
                    auto bytes = reply->readAll();
                    auto w = file.write(bytes);
                    file.close();
                    if (-1 == w) {
                        qWarning() << "failed to write" << file.fileName() << "error" << file.errorString();
                    } else if (w < bytes.size()) {
                        qWarning() << "failed to write to file (no space)" << file.fileName() << "error" << file.errorString() << "file will be removed";
                        file.remove();
                    } else {
                        // done
                    }
                } else {
                    qWarning() << "failed to open" << file.fileName() << "for writing";
                }
            }
        }
        else if (v >= 300 && v < 400) {// Redirection

            // Get the redirection url
            QUrl newUrl = reply->attribute(QNetworkRequest::RedirectionTargetAttribute).toUrl();
            // Because the redirection url can be relative,
            // we have to use the previous one to resolve it
            newUrl = reply->url().resolved(newUrl);

            auto newReply = m_Manager->get(Sc::makeRequest(newUrl));
            m_IconRequests.insert(newReply, r);
        } else  {
            qDebug() << "Http status code:" << v;
        }
    } break;
    default: {
        qDebug() << "Network request failed: " << reply->errorString() << reply->url();
    } break;
    }
}

void
ScVodDataManager::setupDatabase() {
    QSqlQuery q(m_Database);


    static char const* const CreateSql[] = {
        "CREATE TABLE IF NOT EXISTS vod_thumbnails (\n"
        "   id INTEGER PRIMARY KEY,\n"
        "   url TEXT NOT NULL,\n"
        "   file_name TEXT NOT NULL\n"
        ")\n",

        "CREATE TABLE IF NOT EXISTS vod_files (\n"
        "    id INTEGER PRIMARY KEY,\n"
        "    width INTEGER NOT NULL,\n"
        "    height INTEGER NOT NULL,\n"
        "    progress REAL NOT NULL,\n"
        "    format TEXT NOT NULL,\n"
        "    file_name TEXT NOT NULL\n"
        ")\n",

        "CREATE TABLE IF NOT EXISTS vod_url_share (\n"
        "    id INTEGER PRIMARY KEY,\n"
        "    type INTEGER NOT NULL,\n"
        "    length INTEGER NOT NULL DEFAULT 0,\n" // seconds
        "    url TEXT NOT NULL,\n"
        "    video_id TEXT,\n"
        "    title TEXT,\n"
        "    vod_file_id INTEGER REFERENCES vod_files(id) ON DELETE SET NULL\n"
        ")\n",

        "CREATE TABLE IF NOT EXISTS vods (\n"
        "    id INTEGER PRIMARY KEY AUTOINCREMENT,\n"
        "    side1_name TEXT COLLATE NOCASE,\n"
        "    side2_name TEXT COLLATE NOCASE,\n"
        "    side1_race INTEGER,\n"
        "    side2_race INTEGER,\n"
        "    vod_url_share_id INTEGER,\n"
        "    event_name TEXT NOT NULL COLLATE NOCASE,\n"
        "    event_full_name TEXT NOT NULL,\n"
        "    season INTEGER NOT NULL,\n"
        "    stage_name TEXT NOT NULL COLLATE NOCASE,\n"
        "    match_name TEXT NOT NULL,\n"
        "    match_date INTEGER NOT NULL,\n"
        "    game INTEGER NOT NULL,\n"
        "    year INTEGER NOT NULL,\n"
        "    offset INTEGER NOT NULL,\n"
        "    seen INTEGER DEFAULT 0,\n"
        "    match_number INTEGER NOT NULL,\n"
        "    stage_rank INTEGER NOT NULL,\n"
        "    thumbnail_id INTEGER REFERENCES vod_thumbnails(id) ON DELETE SET NULL,\n"
        "    FOREIGN KEY(vod_url_share_id) REFERENCES vod_url_share(id) ON DELETE CASCADE,\n"
        "    UNIQUE (event_name, game, match_date, match_name, match_number, season, stage_name, year) ON CONFLICT REPLACE\n"
        ")\n",

        "CREATE TABLE IF NOT EXISTS settings (\n"
        "    key TEXT NOT NULL UNIQUE,\n"
        "    value TEXT\n"
        ")\n",

        "CREATE TABLE IF NOT EXISTS icons (\n"
        "    url TEXT NOT NULL,\n"
        "    file_name TEXT NOT NULL\n"
        ")\n",

//        "CREATE INDEX IF NOT EXISTS vods_id ON vods (id)\n",
        // must have indices to have decent 'seen' query performance (80+ -> 8 ms)
        "CREATE INDEX IF NOT EXISTS vods_game ON vods (game)\n",
        "CREATE INDEX IF NOT EXISTS vods_year ON vods (year)\n",
        "CREATE INDEX IF NOT EXISTS vods_season ON vods (season)\n",
        "CREATE INDEX IF NOT EXISTS vods_event_name ON vods (event_name)\n",
//        "CREATE INDEX IF NOT EXISTS vods_seen ON vods (seen)\n",



        "CREATE VIEW IF NOT EXISTS offline_vods AS\n"
        "   SELECT\n"
        "       width,\n"
        "       height,\n"
        "       progress,\n"
        "       format,\n"
        "       file_name,\n"
        "       type,\n"
        "       url,\n"
        "       video_id,\n"
        "       title,\n"
        "       vod_file_id,\n"
        "       v.id AS id,\n"
        "       side1_name,\n"
        "       side2_name,\n"
        "       side1_race,\n"
        "       side2_race,\n"
        "       vod_url_share_id,\n"
        "       event_name,\n"
        "       event_full_name,\n"
        "       season,\n"
        "       stage_name,\n"
        "       match_name,\n"
        "       match_date,\n"
        "       game,\n"
        "       year,\n"
        "       offset,\n"
        "       length,\n"
        "       seen,\n"
        "       match_number,\n"
        "       thumbnail_id,\n"
        "       stage_rank\n"
        "   FROM vods v\n"
        "   INNER JOIN vod_url_share u ON v.vod_url_share_id=u.id\n"
        "   INNER JOIN vod_files f ON f.id=u.vod_file_id\n"
        "\n",

        "CREATE VIEW IF NOT EXISTS url_share_vods AS\n"
        "   SELECT\n"
        "       type,\n"
        "       url,\n"
        "       video_id,\n"
        "       title,\n"
        "       vod_file_id,\n"
        "       v.id AS id,\n"
        "       side1_name,\n"
        "       side2_name,\n"
        "       side1_race,\n"
        "       side2_race,\n"
        "       vod_url_share_id,\n"
        "       event_name,\n"
        "       event_full_name,\n"
        "       season,\n"
        "       stage_name,\n"
        "       match_name,\n"
        "       match_date,\n"
        "       game,\n"
        "       year,\n"
        "       offset,\n"
        "       length,\n"
        "       seen,\n"
        "       match_number,\n"
        "       thumbnail_id,\n"
        "       stage_rank\n"
        "   FROM vods v\n"
        "   INNER JOIN vod_url_share u ON v.vod_url_share_id=u.id\n"
        "\n",

    };



    const int Version = 5;


    if (!q.exec("PRAGMA foreign_keys = ON")) {
        qCritical() << "failed to enable foreign keys" << q.lastError();
        setError(Error_CouldntCreateSqlTables);
        return;
    }

    // https://codificar.com.br/blog/sqlite-optimization-faq/
    if (!q.exec("PRAGMA count_changes = OFF")) {
        qCritical() << "failed to disable change counting" << q.lastError();
        setError(Error_CouldntCreateSqlTables);
        return;
    }

    if (!q.exec("PRAGMA temp_store = MEMORY")) {
        qCritical() << "failed to set temp store to memory" << q.lastError();
        setError(Error_CouldntCreateSqlTables);
        return;
    }

    if (!q.exec("PRAGMA journal_mode = WAL")) {
        qCritical() << "failed to set journal mode to WAL" << q.lastError();
        setError(Error_CouldntCreateSqlTables);
        return;
    }

    if (!q.exec("PRAGMA user_version") || !q.next()) {
        qCritical() << "failed to query user version" << q.lastError();
        setError(Error_CouldntCreateSqlTables);
        return;
    }

    auto hasVersion = false;
    auto version = q.value(0).toInt(&hasVersion);
    assert(hasVersion);
    hasVersion = version > 0; // not a newly created db
    auto createTables = true;
    if (hasVersion) {
        if (version != Version) {
            switch (version) {
            case 1:
                updateSql1(q);
            case 2:
                updateSql2(q);
            case 3:
                updateSql3(q);
            case 4:
                updateSql4(q);
            default:
                break;
            }

            if (m_Error) { // don't update version if there is an error
                return;
            }
        } else {
            createTables = false;
        }
    } else {
        if (!q.exec(QStringLiteral("PRAGMA user_version = %1").arg(QString::number(Version)))) {
            qCritical() << "failed to set user version" << q.lastError();
            setError(Error_CouldntCreateSqlTables);
            return;
        }
    }

    qInfo() << "database schema version" << Version;

    if (createTables) {
        for (size_t i = 0; i < _countof(CreateSql); ++i) {
            qDebug() << CreateSql[i];
            if (!q.exec(CreateSql[i])) {
                qCritical() << "failed to create table" << q.lastError();
                setError(Error_CouldntCreateSqlTables);
                return;
            }
        }
    }
}

void
ScVodDataManager::setError(Error error) {
    setState(State_Error);
    if (m_Error != error) {
        m_Error = error;
        emit errorChanged();
    }
}


QString
ScVodDataManager::label(const QString& key, const QVariant& value) const {
    if (value.isValid()) {
        if (key == s_GameKey) {
            int game = value.toInt();
            switch (game) {
            case ScRecord::GameBroodWar:
                return QStringLiteral("Brood War");
            case ScRecord::GameSc2:
                return QStringLiteral("StarCraft II");
            case ScRecord::GameOverwatch:
                return QStringLiteral("Overwatch");
            default:
                return tr("Misc");
            }
        }

        return value.toString();
    }

    if (s_EventNameKey == key) {
        return tr("event");
    }

    return tr(key.toLocal8Bit().data());
}

QString
ScVodDataManager::icon(const QString& key, const QVariant& value) const {
//    ScStopwatch sw("icon", 5);
    if (s_GameKey == key) {
        auto game = value.toInt();
        switch (game) {
        case ScRecord::GameBroodWar:
            return s_BroodWarIconPath;
        case ScRecord::GameSc2:
            return s_Sc2IconPath;
        default:
            return s_RandomIconPath;
        }
    } else if (s_EventNameKey == key) {
        auto event = value.toString();
        QString url;
        if (m_Icons.getIconForEvent(event, &url)) {
            QSqlQuery q(m_Database);
            static const QString sql = QStringLiteral(
                        "SELECT file_name FROM icons WHERE url=?");
            if (!q.prepare(sql)) {
                qCritical() << "failed to prepare select" << q.lastError();
                return false;
            }

            q.addBindValue(url);

            if (!q.exec()) {
                qCritical() << "failed to exec select" << q.lastError();
                return false;
            }

            if (q.next()) {
                auto filePath = m_SharedState->m_IconDir + q.value(0).toString();
                if (QFile::exists(filePath)) {
                    return filePath;
                }
            }
        }
    }

    return s_SailfishIconPath;
}

QString
ScVodDataManager::raceIcon(int race) const
{
    switch (race)
    {
    case ScRecord::RaceProtoss:
        return s_ProtossIconPath;
    case ScRecord::RaceTerran:
        return s_TerranIconPath;
    case ScRecord::RaceZerg:
        return s_ZergIconPath;
    default:
        return QString();
    }
}

void
ScVodDataManager::dropTables() {
    // https://stackoverflow.com/questions/525512/drop-all-tables-command#525523

    // PRAGMA INTEGRITY_CHECK;

    static char const* const DropSql[] = {
        "PRAGMA user_version = 0",
        "PRAGMA writable_schema = 1",
        "DELETE FROM sqlite_master WHERE type IN ('table', 'index', 'trigger')",
        "PRAGMA writable_schema = 0",
        "VACUUM",
    };

    QSqlQuery q(m_Database);
    for (size_t i = 0; i < _countof(DropSql); ++i) {
        qDebug() << DropSql[i];
        if (!q.exec(DropSql[i])) {
            qCritical() << "SQL" << DropSql[i] << "failed" << q.lastError();
            setError(Error_CouldntCreateSqlTables);
        }
    }
}

void
ScVodDataManager::clearCache(ClearFlags flags) {
    QMutexLocker g(&m_Lock);

    RETURN_IF_ERROR;

    // delete files
    if (flags & CF_MetaData) {
        QDir(m_SharedState->m_MetaDataDir).removeRecursively();
        QDir().mkpath(m_SharedState->m_MetaDataDir);
    }

    if (flags & CF_Thumbnails) {
        QDir(m_SharedState->m_ThumbnailDir).removeRecursively();
        QDir().mkpath(m_SharedState->m_ThumbnailDir);
    }

    if (flags & CF_Vods) {
        QDir(m_SharedState->m_VodDir).removeRecursively();
        QDir().mkpath(m_SharedState->m_VodDir);

        tryRaiseVodsChanged();

        emit vodsCleared();
    }

    if (flags & CF_Icons) {
        QDir(m_SharedState->m_IconDir).removeRecursively();
        QDir().mkpath(m_SharedState->m_IconDir);
    }
}

void
ScVodDataManager::clear() {
    QMutexLocker g(&m_Lock);

    RETURN_IF_ERROR;

    clearCache(CF_Everthing);


    dropTables();
    setupDatabase();


    tryRaiseVodsChanged();

    emit vodsCleared();
}

void
ScVodDataManager::excludeEvent(const ScEvent& event, bool* exclude) {
    Q_UNUSED(event);
    Q_ASSERT(exclude);
    *exclude = false;
}
void
ScVodDataManager::excludeStage(const ScStage& stage, bool* exclude) {
    Q_UNUSED(stage);
    Q_ASSERT(exclude);
    *exclude = false;
}

bool
ScVodDataManager::exists(
        const ScEvent& event,
        const ScStage& stage,
        const ScMatch& match,
        bool* _exists) const {

    Q_ASSERT(_exists);

    ScRecord record;
    if (!event.fullName().isEmpty()) {
        record.eventFullName = event.fullName();
        record.valid |= ScRecord::ValidEventFullName;
    }

    if (!event.name().isEmpty()) {
        record.eventName = event.fullName();
        record.valid |= ScRecord::ValidEventName;
    }

    if (event.year() > 0) {
        record.year = event.year();
        record.valid |= ScRecord::ValidYear;
    }

    if (event.season() > 0) {
        record.season = event.season();
        record.valid |= ScRecord::ValidSeason;
    }

    switch (event.game()) {
    case ScEnums::Game_Broodwar:
        record.game = ScRecord::GameBroodWar;
        record.valid |= ScRecord::ValidGame;
        break;
    case ScEnums::Game_Sc2:
        record.game = ScRecord::GameSc2;
        record.valid |= ScRecord::ValidGame;
        break;
    default:
        break;
    }

    if (!stage.name().isEmpty()) {
        record.stage = stage.name();
        record.valid |= ScRecord::ValidStageName;
    }

    if (!match.name().isEmpty()) {
        record.matchName = match.name();
        record.valid |= ScRecord::ValidMatchName;
    }

    if (match.date().isValid()) {
        record.matchDate = match.date();
        record.valid |= ScRecord::ValidMatchDate;
    }

    if (!match.side1().isEmpty()) {
        record.side1Name = match.side1();
        record.side2Name = match.side2();
        record.valid |= ScRecord::ValidSides;
    }

    QSqlQuery q(m_Database);

    qint64 id;
    if (!exists(q, record, &id)) {
        return false;
    }

    *_exists = id != -1;
    return true;
}



bool
ScVodDataManager::exists(QSqlQuery& q, const ScRecord& record, qint64* id) const {

    Q_ASSERT(id);
    Q_ASSERT(record.isValid(
                 ScRecord::ValidEventName |
                 ScRecord::ValidStageName |
                 ScRecord::ValidYear |
//                 ScRecord::ValidGame |
//                 ScRecord::ValidSeason |
                 ScRecord::ValidMatchName |
                 ScRecord::ValidMatchDate));

    *id = -1;

    static const QString sql = QStringLiteral(
                "SELECT\n"
                "    id\n"
                "FROM\n"
                "    vods\n"
                "WHERE\n"
                "    event_name=?\n"
                "    AND year=?\n"
                "    AND game=?\n"
                "    AND season=?\n"
                "    AND match_name=?\n"
                "    AND match_date=?\n"
                "    AND stage_name=?\n");
    if (!q.prepare(sql)) {
        qCritical() << "failed to prepare match select" << q.lastError();
        return false;
    }

    q.addBindValue(record.eventName);
    q.addBindValue(record.year);
    q.addBindValue(record.isValid(ScRecord::ValidGame) ? record.game : -1);
    q.addBindValue(record.isValid(ScRecord::ValidSeason) ? record.season : 1);
    q.addBindValue(record.matchName);
    q.addBindValue(record.matchDate);
    q.addBindValue(record.stage);

    if (!q.exec()) {
        qCritical() << "failed to exec match select" << q.lastError();
        return false;
    }

    if (q.next()) {
        *id = qvariant_cast<qint64>(q.value(0));
    }

    return true;
}

void
ScVodDataManager::excludeMatch(const ScMatch& match, bool* exclude) {
    Q_ASSERT(exclude);

    *exclude = false;

    if (match.isValid()) {
        ScStage stage = match.stage();
        if (stage.isValid()) {
            ScEvent event = stage.event();
            if (event.isValid()) {
                if (!exists(event, stage, match, exclude)) {
                    *exclude = true;
                }
            } else {
                qDebug() << "invalid event";
                *exclude = true;
            }
        } else {
            qDebug() << "invalid stage";
            *exclude = true;
        }
    } else {
        qDebug() << "invalid match";
        *exclude = true;
    }
}

void
ScVodDataManager::hasRecord(const ScRecord& record, bool* _exists) {
    Q_ASSERT(_exists);

    if (record.isValid(ScRecord::ValidEventName |
                       ScRecord::ValidYear |
                       ScRecord::ValidGame |
//                       ScRecord::ValidSeason |
                       ScRecord::ValidStageName |
                       ScRecord::ValidMatchDate |
                       ScRecord::ValidMatchName)) {
        qint64 id = -1;
        QSqlQuery q(m_Database);
        if (!exists(q, record, &id)) {
            *_exists = false;
        } else {
            *_exists = id != -1;
        }
    } else {
        *_exists = false;
    }
}

void
ScVodDataManager::queueVodsToAdd(const QList<ScRecord>& records) {
    if (!records.isEmpty()) {
        QMutexLocker g(&m_Lock);
        QMutexLocker g2(&m_AddQueueLock);
        m_AddQueue << records;
        for (int i = 0; i < records.size(); i += BatchSize) {
            suspendVodsChangedEvents();
            emit vodsToAdd();
        }
    }
}

bool
ScVodDataManager::_addVods(const QList<ScRecord>& records) {
    ScStopwatch sw("_addVods", 10000);
    QSqlQuery q(m_Database);

    if (!q.exec(QStringLiteral("BEGIN IMMEDIATE"))) {
        qCritical() << "failed to start transaction" << q.lastError();
        return false;
    }

    QList<qint64> unknownGameRowids;
    QList<int> unknownGameIndices;
    auto count = 0;
    QString cannonicalUrl, videoId;

    for (int i = 0; i < records.size(); ++i) {
        const ScRecord& record = records[i];

        int urlType, startOffset;

        if (!record.isValid(ScRecord::ValidUrl)) {
            qWarning() << "invalid url" << record;
            continue;
        }

        parseUrl(record.url, &cannonicalUrl, &videoId, &urlType, &startOffset);
        qint64 urlShareId = 0;

        if (UT_Unknown != urlType) {
            static const QString sql = QStringLiteral("SELECT id FROM vod_url_share WHERE video_id=? AND type=?");
            if (!q.prepare(sql)) {
                qCritical() << "failed to prepare query" << q.lastError();
                continue;
            }

            q.addBindValue(videoId);
            q.addBindValue(urlType);

            if (!q.exec()) {
                qCritical() << "failed to exec" << q.lastError();
                continue;
            }

            if (q.next()) {
                urlShareId = qvariant_cast<qint64>(q.value(0));
            } else {
                static const QString sql = QStringLiteral("INSERT INTO vod_url_share (video_id, url, type) VALUES (?, ?, ?)");
                if (!q.prepare(sql)) {
                    qCritical() << "failed to prepare query" << q.lastError();
                    continue;
                }

                q.addBindValue(videoId);
                q.addBindValue(cannonicalUrl);
                q.addBindValue(urlType);

                if (!q.exec()) {
                    qCritical() << "failed to exec" << q.lastError();
                    continue;
                }

                urlShareId = qvariant_cast<qint64>(q.lastInsertId());
            }
        } else {
            static const QString sql = QStringLiteral("INSERT INTO vod_url_share (url, type) VALUES (?, ?)");
            if (!q.prepare(sql)) {
                qCritical() << "failed to prepare query" << q.lastError();
                break;
            }

            q.addBindValue(record.url);
            q.addBindValue(urlType);

            if (!q.exec()) {
                qCritical() << "failed to exec" << q.lastError();
                continue;
            }

            urlShareId = qvariant_cast<qint64>(q.lastInsertId());
        }

        static const QString sql = QStringLiteral(
                    "INSERT INTO vods (\n"
                    "   event_name,\n"
                    "   event_full_name,\n"
                    "   season,\n"
                    "   game,\n"
                    "   year,\n"
                    "   stage_name,\n"
                    "   match_name,\n"
                    "   match_date,\n"
                    "   side1_name,\n"
                    "   side2_name,\n"
                    "   side1_race,\n"
                    "   side2_race,\n"
                    "   vod_url_share_id,\n"
                    "   offset,\n"
                    "   match_number,\n"
                    "   stage_rank\n"
                    ") VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)\n");
        if (!q.prepare(sql)) {
            qCritical() << "failed to prepare vod insert" << q.lastError();
            continue;
        }

        q.addBindValue(record.eventName);
        q.addBindValue(record.eventFullName);
        q.addBindValue(record.isValid(ScRecord::ValidSeason) ? record.season : 1);
        q.addBindValue(record.isValid(ScRecord::ValidGame) ? record.game : -1);
        q.addBindValue(record.year);
        q.addBindValue(record.stage);
        q.addBindValue(record.matchName);
        q.addBindValue(record.matchDate);
        q.addBindValue(record.side1Name);
        q.addBindValue(record.side2Name);
        q.addBindValue(record.side1Race);
        q.addBindValue(record.side2Race);
        q.addBindValue(urlShareId);
        q.addBindValue(startOffset);
        q.addBindValue(record.isValid(ScRecord::ValidMatchNumber) ? record.matchNumber : 1);
        q.addBindValue(record.isValid(ScRecord::ValidStageRank) ? record.stageRank : -1);

        if (!q.exec()) {
            qCritical() << "failed to exec vod insert" << q.lastError();
            continue;
        }

        if (q.lastInsertId().isValid()) {
            ++count;
            if (!record.isValid(ScRecord::ValidGame)) {
                unknownGameRowids << qvariant_cast<qint64>(q.lastInsertId());
                unknownGameIndices << i;
            }
        } else {
            qDebug() << "duplicate record" << record;
        }
    }

    // try to fix game column
    for (int i = 0; i < unknownGameIndices.size(); ++i) {
        const auto index = unknownGameIndices[i];
        const auto rowId = unknownGameRowids[i];
        const ScRecord& record = records[index];

        // see if any record with similar values has a valid game
        static const QString sql = QStringLiteral(
                    "SELECT\n"
                    "    game\n"
                    "FROM\n"
                    "    vods\n"
                    "WHERE\n"
                    "    event_name=?\n"
                    "    AND year=?\n"
                    "    AND season=?\n"
                    "    AND game!=-1\n");
        if (!q.prepare(sql)) {
            qCritical() << "failed to prepare match select" << q.lastError();
            break;
        }

        q.addBindValue(record.eventName);
        q.addBindValue(record.year);
        q.addBindValue(record.isValid(ScRecord::ValidSeason) ? record.season : 1);

        if (!q.exec()) {
            qCritical() << "failed to exec" << q.lastError();
            continue;
        }

        if (q.next()) {
            auto game = q.value(0);

            static const QString sql = QStringLiteral("UPDATE vods SET game=? WHERE id=?");
            if (!q.prepare(sql)) {
                qCritical() << "failed to prepare match select" << q.lastError();
                continue;
            }

            q.addBindValue(game);
            q.addBindValue(rowId);

            if (!q.exec()) {
                qCritical() << "failed to exec" << q.lastError();
                continue;
            }
        }
    }

    if (!q.exec("COMMIT")) {
        qCritical() << "failed to end transaction" << q.lastError();
        return false;
    }

    if (count) {
        emit vodsAdded(count);
    }

    return count > 0;
}



int
ScVodDataManager::fetchMetaData(qint64 rowid) {
    return fetchMetaData(rowid, true);
}

int
ScVodDataManager::fetchMetaDataFromCache(qint64 rowid) {
    return fetchMetaData(rowid, false);
}

int
ScVodDataManager::fetchMetaData(qint64 rowid, bool download) {
    RETURN_MINUS_ON_ERROR;

    ScVodDataManagerWorker::Task t = [=]() {
        m_WorkerInterface->fetchMetaData(rowid, download);
    };
    auto result = m_WorkerInterface->enqueue(std::move(t));
    if (result != -1) {
        emit startWorker();
    }
    return result;
}

int
ScVodDataManager::fetchThumbnail(qint64 rowid) {
    return fetchThumbnail(rowid, true);
}

int
ScVodDataManager::fetchThumbnailFromCache(qint64 rowid) {
    return fetchThumbnail(rowid, false);
}

int
ScVodDataManager::fetchThumbnail(qint64 rowid, bool download) {
    RETURN_MINUS_ON_ERROR;

    ScVodDataManagerWorker::Task t = [=]() {
        m_WorkerInterface->fetchThumbnail(rowid, download);
    };
    auto result = m_WorkerInterface->enqueue(std::move(t));
    if (result != -1) {
        emit startWorker();
    }
    return result;
}



int ScVodDataManager::fetchVod(qint64 rowid, int formatIndex) {
    RETURN_MINUS_ON_ERROR;

    if (formatIndex < 0) {
        qWarning() << "invalid format index, not fetching vod" << formatIndex;
        return -1;
    }

    ScVodDataManagerWorker::Task t = [=]() {
        m_WorkerInterface->fetchVod(rowid, formatIndex);
    };
    auto result = m_WorkerInterface->enqueue(std::move(t));
    if (result != -1) {
        emit startWorker();
    }
    return result;
}

bool
ScVodDataManager::cancelTicket(int ticket)
{
    if (m_WorkerInterface->cancel(ticket)) {
        qDebug() << "canceled ticket" << ticket;
        return true;
    }

    return false;
}

void
ScVodDataManager::cancelFetchVod(qint64 rowid) {
    RETURN_IF_ERROR;

    ScVodDataManagerWorker::Task t = [=]() {
        m_WorkerInterface->cancelFetchVod(rowid);
    };
    auto result = m_WorkerInterface->enqueue(std::move(t));
    if (result != -1) {
        emit startWorker();
    }
}


void
ScVodDataManager::cancelFetchMetaData(int ticket, qint64 rowid) {
    RETURN_IF_ERROR;

    if (ticket >= 0 && m_WorkerInterface->cancel(ticket)) {
        qDebug() << "canceled ticket" << ticket;
        return;
    }

    ScVodDataManagerWorker::Task t = [=]() {
        m_WorkerInterface->cancelFetchMetaData(rowid);
    };

    auto result = m_WorkerInterface->enqueue(std::move(t));
    if (result != -1) {
        emit startWorker();
    }
}

void
ScVodDataManager::cancelFetchThumbnail(int ticket, qint64 rowid) {
    RETURN_IF_ERROR;

    if (ticket >= 0 && m_WorkerInterface->cancel(ticket)) {
        qDebug() << "canceled ticket" << ticket;
        return;
    }

    ScVodDataManagerWorker::Task t = [=]() {
        m_WorkerInterface->cancelFetchThumbnail(rowid);
    };

    auto result = m_WorkerInterface->enqueue(std::move(t));
    if (result != -1) {
        emit startWorker();
    }
}

void
ScVodDataManager::parseUrl(const QString& url, QString* cannonicalUrl, QString* outVideoId, int* outUrlType, int* outStartOffset) {
    Q_ASSERT(outVideoId);
    Q_ASSERT(outUrlType);
    Q_ASSERT(outStartOffset);
    Q_ASSERT(cannonicalUrl);

    QString x = url;

    if (x.startsWith(QStringLiteral("https://youtu.be/"))) {
        // rewrite to facilitate parsing
        // https://youtu.be/FuoK-Z3mTC4?start=22
        x = QStringLiteral("https://www.youtube.com/embed/") + x.mid(17);
    }

    if (x.contains(QStringLiteral("youtube.com"))) {
        *outUrlType = UT_Youtube;
        QUrl u(x);
        // https://www.youtube.com/embed/zoFJFDl5s1Q?start=525
        // https://www.youtube.com/embed/Z-xZG0--jRo
        // https://www.youtube.com/watch?v=FuoK-Z3mTC4
        QUrlQuery q(u);
        if (q.hasQueryItem(QStringLiteral("start"))) {
            *outStartOffset = q.queryItemValue(QStringLiteral("start")).toInt();
        } else {
            *outStartOffset = 0;
        }

        if (q.hasQueryItem(QStringLiteral("v"))) {
            *outVideoId = q.queryItemValue(QStringLiteral("v"));
        } else {
            auto parts = u.path().splitRef(QChar('/'), QString::SkipEmptyParts);
            *outVideoId = parts.last().toString();
        }

        *cannonicalUrl = QStringLiteral("https://www.youtube.com/watch?v=") + *outVideoId;
    } else if (x.contains(QStringLiteral("twitch.tv"))) {
        *outUrlType = UT_Twitch;
        QUrl u(x);
        //https://player.twitch.tv/?video=v123892734&autoplay=false&time=01h15m50s"
        QUrlQuery q(u);
        *outVideoId = q.queryItemValue(QStringLiteral("video"));
        *outStartOffset = parseTwitchOffset(q.queryItemValue(QStringLiteral("time")));
        *cannonicalUrl = QStringLiteral("https://player.twitch.tv/?video=") + *outVideoId;
    } else {
        *outUrlType = UT_Unknown;
        *outStartOffset = 0;
        outVideoId->clear();
        *cannonicalUrl = x;
    }
}

int
ScVodDataManager::parseTwitchOffset(const QString& str) {
    // 01h15m50s
    if (str.isEmpty()) {
        return 0;
    }

    auto hours = str.midRef(0, 2).toInt();
    auto minutes = str.midRef(3, 2).toInt();
    auto seconds = str.midRef(6, 2).toInt();
    return (hours * 60 + minutes) * 60 + seconds;
}

void
ScVodDataManager::deleteVodFiles(qint64 rowid) {
    deleteVodFilesWhere(QStringLiteral("WHERE id=%1").arg(rowid));
}

void
ScVodDataManager::deleteMetaData(qint64 rowid) {

    RETURN_IF_ERROR;


    QMutexLocker g(&m_Lock);

    QSqlQuery q(m_Database);

    // thumbnails
    if (!q.prepare(QStringLiteral(
                      "SELECT\n"
                      "    t.file_name\n"
                      "FROM vods v\n"
                      "INNER JOIN vod_thumbnails t ON v.thumbnail_id=t.id\n"
                      "WHERE v.id=?\n"
                      ))) {
        qCritical() << "failed to prepare query" << q.lastError();
        return;
    }


    q.addBindValue(rowid);

    if (!q.exec()) {
        qCritical() << "failed to exec query" << q.lastError();
        return;
    }

    while (q.next()) {
        auto fileName = q.value(0).toString();
        auto path = m_SharedState->m_ThumbnailDir + fileName;
        if (QFile::remove(path)) {
            qDebug() << "removed" << path;
        }
    }

    // meta data file
    if (!q.prepare(QStringLiteral(
"SELECT u.id FROM vods AS v INNER JOIN vod_url_share "
"AS u ON v.vod_url_share_id=u.id WHERE v.id=?"))) {
        qCritical() << "failed to prepare query" << q.lastError();
        return;
    }

    q.addBindValue(rowid);

    if (!q.exec()) {
        qCritical() << "failed to exec query" << q.lastError();
        return;
    }

    if (!q.next()) {
        qDebug() << "rowid" << rowid << "gone";
        return;
    }

    const auto urlShareId = qvariant_cast<qint64>(q.value(0));
    auto metaDataFilePath = m_SharedState->m_MetaDataDir + QString::number(urlShareId);
    if (QFile::remove(metaDataFilePath)) {
        qDebug() << "removed" << metaDataFilePath;
    }
}

QDate
ScVodDataManager::downloadMarker() const {
    auto str = getPersistedValue(QStringLiteral("download_marker"));
    if (!str.isEmpty()) {
        return QDate::fromString(str, Qt::ISODate);
    }

    return QDate();
}

QString
ScVodDataManager::downloadMarkerString() const {
    return downloadMarker().toString(Qt::ISODate);
}

void
ScVodDataManager::resetDownloadMarker() {
    setDownloadMarker(QDate());
}

void
ScVodDataManager::setDownloadMarker(QDate value) {
    setPersistedValue(QStringLiteral("download_marker"), value.toString(Qt::ISODate));
    emit downloadMarkerChanged();
}

void
ScVodDataManager::suspendVodsChangedEvents() {
    QMutexLocker g(&m_Lock);
    ++m_SuspendedVodsChangedEventCount;
}

void
ScVodDataManager::resumeVodsChangedEvents()
{
    QMutexLocker g(&m_Lock);
    if (--m_SuspendedVodsChangedEventCount == 0 ){
        emit vodsChanged();
    } else if (m_SuspendedVodsChangedEventCount < 0) {
        qCritical() << "m_SuspendedVodsChangedEventCount < 0";
    }
}

void
ScVodDataManager::tryRaiseVodsChanged() {
    if (m_SuspendedVodsChangedEventCount == 0) {
        emit vodsChanged();
    }
}

int
ScVodDataManager::queryVodFiles(qint64 rowid) {
    RETURN_MINUS_ON_ERROR;

    ScVodDataManagerWorker::Task t = [=]() {
        m_WorkerInterface->queryVodFiles(rowid);
    };
    auto result = m_WorkerInterface->enqueue(std::move(t));
    if (result != -1) {
        emit startWorker();
    }
    return result;

}

qreal
ScVodDataManager::seen(const QString& table, const QString& where) const {
    RETURN_IF_ERROR;

    if (table.isEmpty()) {
        qWarning() << "no table";
        return -1;
    }

    if (where.isEmpty()) {
        qWarning() << "no filters";
        return -1;
    }

    QSqlQuery q(m_Database);


    static const QString sql = QStringLiteral(
                "SELECT\n"
                "    SUM(seen)/(1.0 * COUNT(seen))\n"
                "FROM\n"
                "    %1\n"
                "%2\n");

    if (!q.exec(sql.arg(table, where))) {
        qCritical() << "failed to exec query" << q.lastError();
        return -1;
    }

    q.next();

    return q.value(0).toReal();
}

void
ScVodDataManager::setSeen(const QString& table, const QString& where, bool value) {
    RETURN_IF_ERROR;

    if (table.isEmpty()) {
        qWarning() << "no table";
        return;
    }

    if (where.isEmpty()) {
        qWarning() << "no filters";
        return;
    }

    qDebug() << where;

    QMutexLocker g(&m_Lock);

    QSqlQuery q(m_Database);

    QString query;
//    auto beg = filters.cbegin();
//    auto end = filters.cend();

//    for (auto it = beg; it != end; ++it) {
//        if (query.size() > 0) {
//            query += QStringLiteral(" AND ");
//        }

//        query += it.key() + QStringLiteral("=?");
////        qDebug() << it.key() << it.value();
//    }

    query = QStringLiteral("UPDATE vods SET seen=? WHERE id IN (SELECT id FROM %1 %2)").arg(table, where);


    if (!q.prepare(query)) {
        qCritical() << "failed to prepare query" << q.lastError();
        return;
    }

    q.addBindValue(value);

//    for (auto it = beg; it != end; ++it) {
//        q.addBindValue(it.value());
//    }

    if (!q.exec()) {
        qCritical() << "failed to exec query" << q.lastError();
    }
}

void ScVodDataManager::fetchIcons() {
    RETURN_IF_ERROR;

    QMutexLocker g(&m_Lock);

    // also download update if available
    IconRequest r;
    r.url = s_IconsUrl;
    auto reply = m_Manager->get(Sc::makeRequest(r.url));
    m_IconRequests.insert(reply, r);
}

void ScVodDataManager::fetchClassifier() {
    RETURN_IF_ERROR;

    QMutexLocker g(&m_Lock);

    if (!m_ClassfierRequest) {
        m_ClassfierRequest = m_Manager->get(Sc::makeRequest(s_ClassifierUrl));
    }
}

void
ScVodDataManager::fetchSqlPatches() {
    RETURN_IF_ERROR;

    QMutexLocker g(&m_Lock);

    if (!m_SqlPatchesRequest) {
        m_SqlPatchesRequest = m_Manager->get(Sc::makeRequest(s_SqlPatchesUrl));
    }
}

void
ScVodDataManager::batchAddVods(void* ctx) {
    static_cast<ScVodDataManager*>(ctx)->batchAddVods();
}

void
ScVodDataManager::batchAddVods() {

    QList<ScRecord> records;
    for (int i = 0; i < BatchSize && !m_AddQueue.isEmpty(); ++i) {
        records << m_AddQueue.front();
        m_AddQueue.pop_front();
    }

    if (!records.isEmpty() && ready()) {
        _addVods(records);
    }
}


void
ScVodDataManager::vodAddWorkerFinished() {
    QMutexLocker g(&m_Lock);
    resumeVodsChangedEvents();

    emit busyChanged();
}

bool
ScVodDataManager::busy() const {
    QMutexLocker g(&m_AddQueueLock);
    return m_SuspendedVodsChangedEventCount > 0 || !m_AddQueue.isEmpty();
}

QString
ScVodDataManager::makeThumbnailFile(const QString& srcPath) {
    RETURN_IF_ERROR;

    QFile file(srcPath);
    if (file.open(QIODevice::ReadOnly)) {
        auto bytes = file.readAll();
        auto lastDot = srcPath.lastIndexOf(QChar('.'));
        auto extension = srcPath.mid(lastDot);


        QTemporaryFile tempFile(m_SharedState->m_ThumbnailDir + QStringLiteral("XXXXXX") + extension);
        tempFile.setAutoRemove(false);
        if (tempFile.open()) {
            auto thumbNailFilePath = tempFile.fileName();
            if (bytes.size() == tempFile.write(bytes)) {
                return thumbNailFilePath;
            } else {
                tempFile.setAutoRemove(true);
                qWarning() << "failed to write image data to file" << tempFile.errorString();
            }
        } else {
            qWarning() << "failed to allocate temporary file for thumbnail";
        }

    } else {
        qWarning().noquote().nospace() << "failed to open " << srcPath << ", error: " << file.errorString();
    }

    return QString();
}

int
ScVodDataManager::deleteSeenVodFiles(const QString& where) {
    auto w = where.trimmed();
    if (w.startsWith("where ", Qt::CaseInsensitive)) {
        w = w.mid(6).trimmed();
    }

    if (w.isEmpty()) {
        w = "1=1"; // dummy tautology
    }

    QString w2 = QStringLiteral(
"WHERE vod_file_id IN (\n"
"   SELECT DISTINCT vod_file_id FROM offline_vods WHERE vod_url_share_id IN (\n"
"       SELECT DISTINCT vod_url_share_id FROM offline_vods WHERE %1 AND seen=1\n"
"       EXCEPT\n"
"       SELECT DISTINCT vod_url_share_id FROM offline_vods WHERE %1 AND seen=0)\n"
"   )\n").arg(w);

    return deleteVodFilesWhere(w2);
}

int
ScVodDataManager::deleteVodFilesWhere(const QString& where) {
    RETURN_IF_ERROR;

    QMutexLocker g(&m_Lock);

    QSqlQuery q(m_Database);

    if (!q.exec(QStringLiteral("SELECT file_name FROM offline_vods %1").arg(where))) {
        qCritical() << "failed to exec query" << q.lastError();
        return 0;
    }

    auto count = 0;

    while (q.next()) {
        auto fileName = q.value(0).toString();
        if (QFile::remove(m_SharedState->m_VodDir + fileName)) {
            qDebug() << "removed" << m_SharedState->m_VodDir + fileName;
            ++count;
        }
    }

    if (!q.exec(QStringLiteral(
        "DELETE FROM vod_files\n"
        "WHERE\n"
        "   id IN (SELECT vod_file_id FROM offline_vods %1)\n").arg(where))) {
        qCritical() << "failed to exec query" << q.lastError();
        return 0;
    }

    if (count) {
        emit vodsChanged();
    }

    return count;
}


int
ScVodDataManager::vodDownloads() const {
    return m_VodDownloads.size();
}

QVariantList
ScVodDataManager::vodsBeingDownloaded() const {
    return m_VodDownloads;
}

QString
ScVodDataManager::getPersistedValue(const QString& key, const QString& defaultValue) const {
    QSqlQuery q(m_Database);
    if (!q.prepare(QStringLiteral("SELECT value FROM settings WHERE key=?"))) {
        qCritical() << "failed to prepare settings query" << q.lastError();
        return QString();
    }

    q.addBindValue(key);

    if (!q.exec()) {
        qCritical() << "failed to exec query" << q.lastError();
        return QString();
    }

    if (q.next()) {
        return q.value(0).toString();
    }

    return defaultValue;
}

void
ScVodDataManager::setPersistedValue(const QString& key, const QString& value) {
    QSqlQuery q(m_Database);
    setPersistedValue(q, key, value);
}

void
ScVodDataManager::setPersistedValue(QSqlQuery& q, const QString& key, const QString& value) {
    if (!q.prepare(QStringLiteral("INSERT OR REPLACE INTO settings (key, value) VALUES (?, ?)"))) {
        qCritical() << "failed to prepare settings query" << q.lastError();
    }

    q.addBindValue(key);
    q.addBindValue(value);

    if (!q.exec()) {
        qCritical() << "failed to exec query" << q.lastError();
    }
}

void
ScVodDataManager::applySqlPatches(const QByteArray &buffer) {
    QJsonParseError parseError;
    auto doc = QJsonDocument::fromJson(buffer, &parseError);
    if (doc.isEmpty()) {
        qWarning("document is empty, error: %s\n%s\n", qPrintable(parseError.errorString()), qPrintable(buffer));
        return;
    }

    if (!doc.isObject()) {
        qWarning() << "document has no object root" << buffer;
        return;
    }

    auto root = doc.object();

    auto version = root[QStringLiteral("version")].toInt();
    if (version <= 0) {
        qWarning() << "invalid document version" << version << buffer;
        return;
    }

    if (version > 1) {
        qInfo() << "unsupported version" << version << buffer;
        return;
    }

    auto patches = root[QStringLiteral("patches")].toArray();
    QSqlQuery q(m_Database);

    auto patchLevel = sqlPatchLevel();
    qInfo("sql patch level %d\n", patchLevel);

    // apply missing patches
    for (auto i = patchLevel; i < patches.size(); ++i) {
        auto patch = patches[i].toString();
        if (patch.isEmpty()) {
            qWarning() << "patch array contains empty strings" << i << version << buffer;
            return;
        }

        qDebug() << "applying" << patch;

        if (!q.exec(QStringLiteral("BEGIN"))) {
            qWarning() << "failed to start transaction" << i << q.lastError();
            return;
        }

        if (!q.exec(patch)) {
            qWarning() << "failed to apply patch" << i << patch << q.lastError();
            q.exec(QStringLiteral("ROLLBACK"));
            return;
        }

        setPersistedValue(q, s_SqlPatchLevelKey, QString::number(i + 1));

        if (!q.exec(QStringLiteral("COMMIT"))) {
            qWarning() << "failed to commit transaction" << i << q.lastError();
            return;
        }

        qDebug() << "patch level now" << (i + 1);

        emit sqlPatchLevelChanged();
    }

    emit vodsChanged();
}


void
ScVodDataManager::resetSqlPatchLevel() {
    setPersistedValue(s_SqlPatchLevelKey, s_SqlPatchLevelDefault);
}

void
ScVodDataManager::updateSql1(QSqlQuery& q) {
    qInfo("Begin update database v1 to v2\n");

    const auto marker = downloadMarker();

    static char const* const Sql[] = {
        "BEGIN",
        "DROP TABLE IF EXISTS settings\n",
        "CREATE TABLE settings (\n"
        "    key TEXT NOT NULL UNIQUE,\n"
        "    value TEXT\n"
        ")\n",
    };

    // can't use new object, table will be locked
//    QSqlQuery q(m_Database);

    for (size_t i = 0; i < _countof(Sql); ++i) {
        qDebug() << Sql[i];
        if (!q.exec(Sql[i])) {
            qCritical() << "failed to upgrade database from v1 to v2" << q.lastError();
            setError(Error_CouldntCreateSqlTables);
            return;
        }
    }

    setDownloadMarker(marker);


    static char const* const Sql2[] = {
        "PRAGMA user_version = 2",
        "COMMIT",
    };

    for (size_t i = 0; i < _countof(Sql2); ++i) {
        qDebug() << Sql2[i];
        if (!q.exec(Sql2[i])) {
            qCritical() << "failed to upgrade database from v1 to v2" << q.lastError();
            goto Error;
        }
    }

    qInfo("Update database v1 to v2 completed successfully\n");
Exit:
    return;
Error:
    q.exec("ROLLBACK");
    setError(Error_CouldntCreateSqlTables);
    goto Exit;
}

void
ScVodDataManager::updateSql2(QSqlQuery& q) {
    qInfo("Begin update database v2 to v3\n");

    static char const* const Sql1[] = {
        "BEGIN",
        "ALTER TABLE vod_url_share ADD COLUMN vod_file_id INTEGER REFERENCES vod_files(id) ON DELETE SET NULL",
    };

    for (size_t i = 0; i < _countof(Sql1); ++i) {
        qDebug() << Sql1[i];
        if (!q.exec(Sql1[i])) {
            qCritical() << "failed to upgrade database from v2 to v3" << q.lastError();
            goto Error;
        }
    }

    if (!q.exec(QStringLiteral("SELECT vod_file_id, v.vod_url_share_id FROM vods v INNER JOIN vod_file_ref r ON v.id=r.vod_id INNER JOIN vod_files f ON f.id=r.vod_file_id"))) {
        qCritical() << "failed to exec select" << q.lastError();
        goto Error;
    }

    while (q.next()) {
        auto vodFileId = qvariant_cast<qint64>(q.value(0));
        auto vodUrlShareId = qvariant_cast<qint64>(q.value(1));
        QSqlQuery q2(m_Database);
        if (!q2.prepare(QStringLiteral("UPDATE vod_url_share SET vod_file_id=? WHERE id=?"))) {
            qCritical() << "failed to prepare settings query" << q2.lastError();
            goto Error;
        }

        q2.addBindValue(vodFileId);
        q2.addBindValue(vodUrlShareId);

        if (!q2.exec()) {
            qCritical() << "failed to exec query" << q2.lastError();
            goto Error;
        }
    }

    static char const* const Sql2[] = {
        "DROP VIEW IF EXISTS offline_vods",
        "DROP TABLE IF EXISTS vod_file_ref",
        "PRAGMA user_version = 3",
        "COMMIT",
        "VACUUM",
    };

    for (size_t i = 0; i < _countof(Sql2); ++i) {
        qDebug() << Sql2[i];
        if (!q.exec(Sql2[i])) {
            qCritical() << "failed to upgrade database from v2 to v3" << q.lastError();
            goto Error;
        }
    }

    qInfo("Update database v2 to v3 completed successfully\n");
Exit:
    return;
Error:
    q.exec("ROLLBACK");
    setError(Error_CouldntCreateSqlTables);
    goto Exit;
}





void
ScVodDataManager::updateSql3(QSqlQuery& q) {
    qInfo("Begin update database v3 to v4\n");

    static char const* const Sql1[] = {
        "BEGIN",
        "ALTER TABLE vod_url_share ADD COLUMN length INTEGER NOT NULL DEFAULT 0",
    };

    for (size_t i = 0; i < _countof(Sql1); ++i) {
        qDebug() << Sql1[i];
        if (!q.exec(Sql1[i])) {
            qCritical() << "failed to upgrade database from v3 to v4" << q.lastError();
            goto Error;
        }
    }

    // find all existing vod files and update length from vod meta data
    if (!q.exec(QStringLiteral("SELECT id FROM vod_url_share"))) {
        qCritical() << "failed to exec select" << q.lastError();
        goto Error;
    }

    qDebug() << "Updating length column from vod meta data";
    while (q.next()) {
        auto urlShareId = qvariant_cast<qint64>(q.value(0));
        auto metaDataFilePath = m_SharedState->m_MetaDataDir + QString::number(urlShareId);
        if (QFileInfo::exists(metaDataFilePath)) {
            QFile file(metaDataFilePath);
            if (file.open(QIODevice::ReadOnly)) {
                VMVod vod;
                {
                    QDataStream s(&file);
                    s >> vod;
                }

                if (vod.isValid()) {
                    QSqlQuery q2(m_Database);
                    if (!q2.prepare(QStringLiteral("UPDATE vod_url_share SET length=? WHERE id=?"))) {
                        qCritical() << "failed to prepare settings query" << q2.lastError();
                        goto Error;
                    }

                    q2.addBindValue(vod.description().duration());
                    q2.addBindValue(urlShareId);

                    if (!q2.exec()) {
                        qCritical() << "failed to exec query" << q2.lastError();
                        goto Error;
                    }

                    qDebug() << "updated id" << urlShareId << "length" << vod.description().duration();
                } else {
                    qDebug() << "removing bad meta data file" << metaDataFilePath;
                    file.close();
                    file.remove();
                }
            } else {
                qDebug() << "failed to open existing meta data file" << metaDataFilePath << "removing file";
                file.remove();
            }
        }
    }

    static char const* const Sql2[] = {
        "DROP VIEW IF EXISTS offline_vods",
        "PRAGMA user_version = 4",
        "COMMIT",
        "VACUUM",
    };

    for (size_t i = 0; i < _countof(Sql2); ++i) {
        qDebug() << Sql2[i];
        if (!q.exec(Sql2[i])) {
            qCritical() << "failed to upgrade database from v3 to v4" << q.lastError();
            goto Error;
        }
    }

    qInfo("Update database v3 to v4 completed successfully\n");
Exit:
    return;
Error:
    q.exec("ROLLBACK");
    setError(Error_CouldntCreateSqlTables);
    goto Exit;
}


void
ScVodDataManager::updateSql4(QSqlQuery& q) {
    qInfo("Begin update of database v4 to v5\n");

    static char const* const Sql[] = {
        "BEGIN",
        "ALTER TABLE vods ADD COLUMN stage_rank INTEGER NOT NULL DEFAULT -1",
        "DROP VIEW IF EXISTS offline_vods",
        "DROP VIEW IF EXISTS url_share_vods",
        "PRAGMA user_version = 5",
        "COMMIT",
        "VACUUM",
    };

    for (size_t i = 0; i < _countof(Sql); ++i) {
        qDebug() << Sql[i];
        if (!q.exec(Sql[i])) {
            qCritical() << "Failed to update database from v4 to v5" << q.lastError();
            goto Error;
        }
    }

    qInfo("Update of database v4 to v5 completed successfully\n");
Exit:
    return;
Error:
    q.exec("ROLLBACK");
    setError(Error_CouldntCreateSqlTables);
    goto Exit;
}

int
ScVodDataManager::sqlPatchLevel() const
{
    return getPersistedValue(s_SqlPatchLevelKey, s_SqlPatchLevelDefault).toInt();
}

int
ScVodDataManager::vodEndOffset(qint64 rowid, int offset, int vodLengthS) const
{
    static const QString sql = QStringLiteral(
"SELECT\n"
"   MIN(offset)\n"
"FROM\n"
"   offline_vods\n"
"WHERE\n"
"   offset>?\n"
"   AND vod_url_share_id IN (SELECT vod_url_share_id FROM offline_vods WHERE id=?)\n");

    QSqlQuery q(m_Database);
    if (!q.prepare(sql)) {
        qCritical() << "failed to prepare query" << q.lastError();
        return -1;
    }

    q.addBindValue(offset);
    q.addBindValue(rowid);

    if (!q.exec()) {
        qCritical() << "failed to exec query" << q.lastError();
        return -1;
    }

    if (q.next()) {
        auto v = q.value(0);
        if (!v.isNull()) {
            return v.toInt();
        }
    }

    return vodLengthS;
}

void
ScVodDataManager::setState(State state) {
    m_State = state;
}

bool
ScVodDataManager::ready() const {
    return State_Ready == m_State;
}

QString ScVodDataManager::dataDirectory() const
{
    return getPersistedValue(QStringLiteral(DATA_DIR_KEY), QStandardPaths::writableLocation(QStandardPaths::CacheLocation));
}

void ScVodDataManager::setDataDirectory(const QString& value)
{
    return setPersistedValue(QStringLiteral(DATA_DIR_KEY), value);
}

void ScVodDataManager::setDirectories()
{
    const auto dataDir = dataDirectory();
    m_SharedState->m_MetaDataDir = dataDir + QStringLiteral("/meta/");
    m_SharedState->m_ThumbnailDir = dataDir + QStringLiteral("/thumbnails/");
    m_SharedState->m_VodDir = dataDir + QStringLiteral("/vods/");
    m_SharedState->m_IconDir = dataDir + QStringLiteral("/icons/");
}

void ScVodDataManager::moveDataDirectory(const QString& _targetDirectory)
{
    QMutexLocker g(&m_Lock);
    RETURN_IF_ERROR;

    if (busy()) {
        qWarning() << "data manager is busy; refusing to move directory to" << _targetDirectory;
        return;
    }

    if (_targetDirectory.isEmpty()) {
        qWarning() << "empty target dir";
        return;
    }

    QString targetDirectory = _targetDirectory;
    if (targetDirectory.endsWith('/')) {
        if (targetDirectory.size() == 1) {
            qWarning() << "refusing to move to root directory";
            return;
        }

        targetDirectory.chop(1);
    }

    // unblock when done
    suspendVodsChangedEvents();

    m_DataDirectoryMover = new DataDirectoryMover;
    m_DataDirectoryMover->moveToThread(&m_AddThread);
    connect(&m_AddThread, &QThread::finished, m_DataDirectoryMover, &QObject::deleteLater);
    connect(m_DataDirectoryMover, &DataDirectoryMover::dataDirectoryChanging, this, &ScVodDataManager::onDataDirectoryChanging, Qt::QueuedConnection);

    QMetaObject::invokeMethod(m_DataDirectoryMover, "move", Qt::QueuedConnection,
                               Q_ARG( QString, dataDirectory() ),
                              Q_ARG( QString, targetDirectory ));
}

void
ScVodDataManager::cancelMoveDataDirectory() {
    QMutexLocker g(&m_Lock);

    if (m_DataDirectoryMover) {
        qDebug() << "canceling in progress data directory move";
        QMetaObject::invokeMethod(m_DataDirectoryMover, "cancel", Qt::QueuedConnection);
    } else {
        qDebug() << "no data directory move in progress";
    }
}

void
ScVodDataManager::onDataDirectoryChanging(int changeType, QString path, float progress, int error, QString errorDescription) {
    QMutexLocker g(&m_Lock);
    if (DDCT_Finished == changeType) {
        m_DataDirectoryMover = nullptr;
        switch (error) {
        case Error_None:
        case Error_Warning:
            setDataDirectory(path);
            setDirectories();
            break;
        }

        resumeVodsChangedEvents();
    }

    emit dataDirectoryChanging(changeType, path, progress, error, errorDescription);
}

ScClassifier*
ScVodDataManager::classifier() const
{
    return &const_cast<ScVodDataManager*>(this)->m_Classifier;
}

void ScVodDataManager::setMaxConcurrentMetaDataDownloads(int value)
{
    if (value <= 0) {
        qWarning() << "invalid value for max concurrent metadata downloads" << value;
        return;
    }

    if (value != m_MaxConcurrentMetaDataDownloads) {
        m_MaxConcurrentMetaDataDownloads = value;
        emit maxConcurrentMetaDataDownloadsChanged(value);
    }
}

int
ScVodDataManager::fetchTitle(qint64 rowid)
{
    RETURN_MINUS_ON_ERROR;

    ScVodDataManagerWorker::Task t = [=]() {
        m_WorkerInterface->fetchTitle(rowid);
    };
    auto result = m_WorkerInterface->enqueue(std::move(t));
    if (result != -1) {
        emit startWorker();
    }
    return result;
}

int
ScVodDataManager::fetchSeen(qint64 rowId, const QString& table, const QString& where)
{
    RETURN_MINUS_ON_ERROR;

    ScVodDataManagerWorker::Task t = [=]() {
        m_WorkerInterface->fetchSeen(rowId, table, where);
    };
    auto result = m_WorkerInterface->enqueue(std::move(t));
    if (result != -1) {
        emit startWorker();
    }
    return result;
}

int
ScVodDataManager::fetchVodEnd(qint64 rowid, int startOffsetS, int vodLengthS)
{
    RETURN_MINUS_ON_ERROR;

    ScVodDataManagerWorker::Task t = [=]() {
        m_WorkerInterface->fetchVodEnd(rowid, startOffsetS, vodLengthS);
    };
    auto result = m_WorkerInterface->enqueue(std::move(t));
    if (result != -1) {
        emit startWorker();
    }
    return result;
}

void
ScVodDataManager::onVodDownloadsChanged(ScVodIdList ids)
{
    m_VodDownloads.clear();
    m_VodDownloads.reserve(ids.length());

    foreach (auto id, ids) {
        m_VodDownloads << id;
    }

    emit vodDownloadsChanged();
}

void
ScVodDataManager::deleteVod(qint64 rowid)
{
    RETURN_IF_ERROR;

    QSqlQuery q(m_Database);
    if (!q.prepare(QStringLiteral("DELETE FROM vods WHERE id=?"))) {
        qCritical() << "failed to prepare query" << q.lastError();
        return;
    }

    q.addBindValue(rowid);

    if (!q.exec()) {
        qCritical() << "failed to exec query" << q.lastError();
        return;
    }
}

void
ScVodDataManager::deleteThumbnail(qint64 rowid)
{
    RETURN_IF_ERROR;

    QSqlQuery q(m_Database);
    if (!q.prepare(QStringLiteral("SELECT thumbnail_id, file_name FROM vod_thumbnails t INNER JOIN vods v ON t.id=v.thumbnail_id WHERE v.id=?"))) {
        qCritical() << "failed to prepare query" << q.lastError();
        return;
    }

    q.addBindValue(rowid);

    if (!q.exec()) {
        qCritical() << "failed to exec query" << q.lastError();
        return;
    }

    if (q.next()) {
        auto thumbnailId = q.value(0);
        auto filePath = m_SharedState->m_ThumbnailDir + q.value(1).toString();
        QFile::remove(filePath);

        if (!q.prepare(QStringLiteral("DELETE from vod_thumbnails WHERE id=?"))) {
            qCritical() << "failed to prepare query" << q.lastError();
            return;
        }

        q.addBindValue(thumbnailId);

        if (!q.exec()) {
            qCritical() << "failed to exec query" << q.lastError();
            return;
        }
    }
}

QVariant
ScVodDataManager::databaseVariant() const
{
    return QVariant::fromValue(m_Database);
}
