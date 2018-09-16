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
#include "ScVodman.h"
#include "Vods.h"
#include "Sc.h"
#include "ScRecord.h"

#include <vodman/VMVodFileDownload.h>

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

#ifndef _countof
#   define _countof(x) (sizeof(x)/sizeof((x)[0]))
#endif

#define RETURN_IF_ERROR \
    do { \
        if (m_State != State_Ready) { \
            qCritical("Not ready, return"); \
        } \
    } while (0)


#define CLASSIFIER_FILE "/classifier.json.gz"
#define ICONS_FILE "/icons.json.gz"
#define SQL_PATCH_FILE "/sql_patches.json.gz"

namespace  {

const QString s_IconsUrl = QStringLiteral("https://www.dropbox.com/s/p2640l82i3u1zkg/icons.json.gz?dl=1");
const QString s_ClassifierUrl = QStringLiteral("https://www.dropbox.com/s/3cckgyzlba8kev9/classifier.json.gz?dl=1");
const QString s_SqlPatchesUrl = QStringLiteral("https://www.dropbox.com/s/sip5esgcba6hwfo/sql_patches.json.gz?dl=1");
const QString s_SqlPatchLevelKey = QStringLiteral("sql_patch_level");
const QString s_SqlPatchLevelDefault = QStringLiteral("0");
const QString s_ProtossIconPath = QStringLiteral(QT_STRINGIFY(SAILFISH_DATADIR) "/media/protoss.png");
const QString s_TerranIconPath = QStringLiteral(QT_STRINGIFY(SAILFISH_DATADIR) "/media/terran.png");
const QString s_ZergIconPath = QStringLiteral(QT_STRINGIFY(SAILFISH_DATADIR) "/media/zerg.png");

const int BatchSize = 1<<13;

bool selectIdFromVodsWhereUrlShareIdEquals(QSqlQuery& q, qint64 urlShareId) {
    static const QString s_SelectIdFromVodsWhereVodUrlShareId = QStringLiteral("SELECT id FROM vods WHERE vod_url_share_id=?");
    if (!q.prepare(s_SelectIdFromVodsWhereVodUrlShareId)) {
        qCritical() << "failed to prepare query" << q.lastError();
        return false;
    }

    q.addBindValue(urlShareId);

    if (!q.exec()) {
        qCritical() << "failed to exec query" << q.lastError();
        return false;
    }

    return true;
}

class Stopwatch {
public:
    ~Stopwatch() {
        auto stop = QDateTime::currentMSecsSinceEpoch();
        auto duration = stop - m_Start;
        if (duration > m_Threshold) {
            qWarning("%s took %d [ms]\n", qPrintable(m_Name), (int)duration);
        } else {
//            qDebug("%s took %d [ms]\n", qPrintable(m_Name), (int)duration);
        }
    }
    Stopwatch(const char* name, int millisBeforeWarn = 100)
        : m_Name(QLatin1String(name))
        , m_Start(QDateTime::currentMSecsSinceEpoch())
        , m_Threshold(millisBeforeWarn)
    {

    }

    Stopwatch(const QString& str, int millisBeforeWarn = 100)
        : m_Name(str)
        , m_Start(QDateTime::currentMSecsSinceEpoch())
        , m_Threshold(millisBeforeWarn)
    {

    }

private:
    QString m_Name;
    qint64 m_Start;
    int m_Threshold;
};

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

bool DataDirectoryMover::mountPoint(const QString& dir, QByteArray* mountPoint) {
    QProcess process;
    QStringList args;
    args << QStringLiteral("-L") << QStringLiteral("-c") << QStringLiteral("%m") << dir;
    process.start(QStringLiteral("stat"), args, QIODevice::ReadOnly);

    // Wait for it to start
    if (!process.waitForStarted()) {
        emit dataDirectoryChanging(ScVodDataManager::DDCT_Finished, dir, 0, ScVodDataManager::Error_ProcessOutOfResources, QStringLiteral("Failed to start process"));
        return false;
    }

    if (!process.waitForFinished()) {
        process.kill();
        emit dataDirectoryChanging(ScVodDataManager::DDCT_Finished, dir, 0, ScVodDataManager::Error_ProcessTimeout, QStringLiteral("Timeout after 30 s"));
        return false;
    }

    if (process.exitStatus() != QProcess::NormalExit) {
        emit dataDirectoryChanging(ScVodDataManager::DDCT_Finished, dir, 0, ScVodDataManager::Error_ProcessCrashed, QString());
        return false;
    }

    if (process.exitCode() != 0) {
        process.setReadChannel(QProcess::StandardError);
        emit dataDirectoryChanging(ScVodDataManager::DDCT_Finished, dir, 0, ScVodDataManager::Error_ProcessFailed, QString::fromLocal8Bit(process.readAll()));
        return false;
    }

    *mountPoint = process.readAll();
    if (mountPoint->isEmpty() || (*mountPoint)[0] != '/') {
        emit dataDirectoryChanging(
                    ScVodDataManager::DDCT_Finished,
                    dir,
                    0,
                    ScVodDataManager::Error_ProcessFailed,
                    QStringLiteral("'stat %1' output: '%2'").arg(args.join(" "), QString::fromLocal8Bit(*mountPoint)));
        return false;
    }

    return true;
}

void DataDirectoryMover::_move(const QString& currentDir, const QString& targetDir) {
    QDir d(QDir(targetDir).canonicalPath());
    if (!d.exists()) {
        if (!d.mkpath(d.absolutePath())) {
            qWarning() << "failed to create" << d.absolutePath();
            emit dataDirectoryChanging(ScVodDataManager::DDCT_Finished, targetDir, 0, ScVodDataManager::Error_ProcessOutOfResources, QStringLiteral("Failed to create target directory").arg(targetDir));
            return;
        }
    }

    QByteArray currentMountPoint, targetMountPoint;


    if (!mountPoint(currentDir, &currentMountPoint)) {
        return;
    }

    if (!mountPoint(targetDir, &targetMountPoint)) {
        return;
    }

    const QString names[] = {
        QStringLiteral("/meta/"),
        QStringLiteral("/thumbnails/"),
        QStringLiteral("/vods/"),
        QStringLiteral("/icons/"),
    };

    if (targetMountPoint == currentMountPoint) {
        // move dirs
        QDir d;
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

            args.clear();
            args << QStringLiteral("-a") << src << targetDir;
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

            if (process.exitCode() != 0) {
                auto errorString = process.readAllStandardError();
                emit dataDirectoryChanging(
                            ScVodDataManager::DDCT_Finished,
                            src,
                            0.25f * i,
                            ScVodDataManager::Error_ProcessFailed,
                            QStringLiteral("'rsync %1' failed: %2").arg(args.join(" "), QString::fromLocal8Bit(errorString)));
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
                QString(),
                1,
                0,
                QString());
}

void DataDirectoryMover::cancel() {
    m_Canceled = 1;
}

//void DataDirectoryMover::onProcessFinished(int, QProcess::ExitStatus) {

//}
//void DataDirectoryMover::onProcessError(QProcess::ProcessError) {

//}

void DataDirectoryMover::onProcessReadyRead() {
    QProcess* process = qobject_cast<QProcess*>(QObject::sender());
    Q_ASSERT(process);

    if (0 != m_Canceled) {
        process->kill();
    }
}

////////////////////////////////////////////////////////////////////////////////


ScVodDataManager::~ScVodDataManager() {
    qDebug() << "dtor entry";

    setState(State_Finalizing);
    qDebug() << "set finalizing";

    if (m_Vodman) {
        delete m_Vodman;
        m_Vodman = nullptr;
        qDebug() << "deleted vodman";
    }

    {
        QMutexLocker guard(&m_Lock);

        if (m_ClassfierRequest) {
            m_ClassfierRequest->abort();
            qDebug() << "aborted classifier request";
        }


        if (m_SqlPatchesRequest) {
            m_SqlPatchesRequest->abort();
            qDebug() << "aborted sql patches request";
        }

        foreach (auto& key, m_ThumbnailRequests.keys()) {
            key->abort();
        }
        qDebug() << "aborted thumbnail requests";

        foreach (auto& key, m_IconRequests.keys()) {
            key->abort();
        }
        qDebug() << "aborted icons requests";
    }

    m_AddThread.quit();
    m_AddThread.wait();

    while (true) {
        {
            QMutexLocker guard(&m_Lock);

            if (!m_ClassfierRequest &&
                !m_SqlPatchesRequest &&
                 m_ThumbnailRequests.isEmpty() &&
                 m_IconRequests.isEmpty())
            {
                break;
            }
        }

        QThread::msleep(100);
        qDebug() << "wait for requests";
        qDebug() << "classifier req gone" << (m_ClassfierRequest == nullptr);
        qDebug() << "sql patch req gone" << (m_SqlPatchesRequest == nullptr);
        qDebug() << "thumbnail reqs gone" << (m_ThumbnailRequests.isEmpty());
        qDebug() << "icon reqs gone" << (m_IconRequests.isEmpty());
    }

    {
        QMutexLocker guard(&m_Lock);
        if (m_Manager) {
            delete m_Manager;
            m_Manager = nullptr;
        }
        qDebug() << "deleted network manager";
    }

    qDebug() << "dtor exit";
}

ScVodDataManager::ScVodDataManager(QObject *parent)
    : QObject(parent)
    , m_Lock(QMutex::Recursive)
    , m_AddQueueLock(QMutex::NonRecursive)
    , m_Vodman(nullptr)
    , m_Manager(nullptr)
    , m_ClassfierRequest(nullptr)
    , m_SqlPatchesRequest(nullptr)
    , m_DataDirectoryMover(nullptr)
    , m_State(State_Initializing)
{
    m_Manager = new QNetworkAccessManager(this);
    connect(m_Manager, &QNetworkAccessManager::finished, this,
            &ScVodDataManager::requestFinished);

    m_Vodman = new ScVodman(this);
    connect(m_Vodman, &ScVodman::metaDataDownloadCompleted,
            this, &ScVodDataManager::onMetaDataDownloadCompleted);
    connect(m_Vodman, &ScVodman::fileDownloadChanged,
            this, &ScVodDataManager::onFileDownloadChanged);
    connect(m_Vodman, &ScVodman::fileDownloadCompleted,
            this, &ScVodDataManager::onFileDownloadCompleted);
    connect(m_Vodman, &ScVodman::downloadFailed,
            this, &ScVodDataManager::onDownloadFailed);

    m_MetaDataDir = QStandardPaths::writableLocation(QStandardPaths::CacheLocation) + "/meta/";
    m_ThumbnailDir = QStandardPaths::writableLocation(QStandardPaths::CacheLocation) + "/thumbnails/";
    m_VodDir = QStandardPaths::writableLocation(QStandardPaths::CacheLocation) + "/vods/";
    m_IconDir = QStandardPaths::writableLocation(QStandardPaths::CacheLocation) + "/icons/";

    m_Error = Error_None;
    m_SuspendedVodsChangedEventCount = 0;

    const auto databaseDir = QStandardPaths::writableLocation(QStandardPaths::DataLocation);

    QDir d;
    d.mkpath(databaseDir);

    auto databaseFilePath = databaseDir + "/vods.sqlite";
    m_Database = QSqlDatabase::addDatabase("QSQLITE", databaseFilePath);
    m_Database.setDatabaseName(databaseFilePath);
    if (m_Database.open()) {
        qInfo("Opened database %s\n", qPrintable(databaseFilePath));
        setupDatabase();
    } else {
        qCritical("Could not open database '%s'. Error: %s\n", qPrintable(databaseFilePath), qPrintable(m_Database.lastError().text()));
        setError(Error_Unknown);
        return;
    }

    const auto dataDir = dataDirectory();
    m_MetaDataDir = dataDir + QStringLiteral("/meta/");
    m_ThumbnailDir = dataDir + QStringLiteral("/thumbnails/");
    m_VodDir = dataDir + QStringLiteral("/vods/");
    m_IconDir = dataDir + QStringLiteral("/icons/");

    d.mkpath(m_MetaDataDir);
    d.mkpath(m_ThumbnailDir);
    d.mkpath(m_VodDir);
    d.mkpath(m_IconDir);


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

    Stopwatch sw("requestFinished");
    QMutexLocker guard(&m_Lock);
    reply->deleteLater();

    auto it = m_ThumbnailRequests.find(reply);
    if (it != m_ThumbnailRequests.end()) {
        auto r = it.value();
        m_ThumbnailRequests.erase(it);

        RETURN_ON_REQUEST;

        thumbnailRequestFinished(reply, r);
    } else {
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
    }

#undef RETURN_ON_REQUEST
}

void
ScVodDataManager::thumbnailRequestFinished(QNetworkReply* reply, ThumbnailRequest& r) {
    switch (reply->error()) {
    case QNetworkReply::OperationCanceledError:
        break;
    case QNetworkReply::NoError: {
        // Get the http status code
        int v = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        if (v >= 200 && v < 300) {// Success

            // Here we got the final reply
            auto bytes = reply->readAll();
            addThumbnail(r.rowid, bytes);

        }
        else if (v >= 300 && v < 400) {// Redirection

            // Get the redirection url
            QUrl newUrl = reply->attribute(QNetworkRequest::RedirectionTargetAttribute).toUrl();
            // Because the redirection url can be relative,
            // we have to use the previous one to resolve it
            newUrl = reply->url().resolved(newUrl);

            m_ThumbnailRequests.insert(m_Manager->get(Sc::makeRequest(newUrl)), r);
        } else  {
            qDebug() << "Http status code:" << v;
        }
    } break;
    default: {
        qDebug() << "Network request failed: " << reply->errorString() << reply->url();

        QSqlQuery q(m_Database);
        if (!q.prepare(QStringLiteral("SELECT id FROM vods WHERE thumbnail_id=?"))) {
            qCritical() << "failed to prepare query" << q.lastError();
            return;
        }

        q.addBindValue(r.rowid);

        if (!q.exec()) {
            qCritical() << "failed to exec query" << q.lastError();
            return;
        }

        auto url = reply->url().toString();
        while (q.next()) {
            auto vodRowId = qvariant_cast<qint64>(q.value(0));
            emit thumbnailDownloadFailed(vodRowId, reply->error(), url);
        }
    } break;
    }
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
                            for (auto i = 0; i < m_Icons.size(); ++i) {
                                auto url = m_Icons.url(i);

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
                                    auto filePath = m_IconDir + q.value(0).toString();
                                    QFileInfo fi(filePath);
                                    if (fi.exists() && fi.size() > 0) {
                                        continue;
                                    }
                                } else {
                                    auto extension = m_Icons.extension(i);
                                    QTemporaryFile tempFile(m_IconDir + QStringLiteral("XXXXXX") + extension);
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

                auto iconFilePath = m_IconDir + q.value(0).toString();

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
        "       thumbnail_id\n"
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
        "       thumbnail_id\n"
        "   FROM vods v\n"
        "   INNER JOIN vod_url_share u ON v.vod_url_share_id=u.id\n"
        "\n",

    };



    const int Version = 4;


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

    bool hasVersion = false;
    auto version = q.value(0).toInt(&hasVersion);
    if (hasVersion && version != Version) {

        switch (version) {
        case 1:
            updateSql1(q);
        case 2:
            updateSql2(q);
        case 3:
            updateSql3(q);
        default:
            break;
        }

        if (m_Error) { // don't update version if there is an error
            return;
        }

        if (!q.exec(QStringLiteral("PRAGMA user_version = %1").arg(QString::number(Version)))) {
            qCritical() << "failed to set user version" << q.lastError();
            setError(Error_CouldntCreateSqlTables);
            return;
        }
    }

    for (size_t i = 0; i < _countof(CreateSql); ++i) {
//        qDebug() << CreateSql[i];
        if (!q.exec(CreateSql[i])) {
            qCritical() << "failed to create table" << q.lastError();
            setError(Error_CouldntCreateSqlTables);
            return;
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
        if (key == QStringLiteral("game")) {
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

    if (QStringLiteral("event_name") == key) {
        return tr("event");
    }

    return tr(key.toLocal8Bit().data());
}

QString
ScVodDataManager::icon(const QString& key, const QVariant& value) const {
//    Stopwatch sw("icon", 5);
    if (QStringLiteral("game") == key) {
        auto game = value.toInt();
        switch (game) {
        case ScRecord::GameBroodWar:
            return QStringLiteral(QT_STRINGIFY(SAILFISH_DATADIR) "/media/bw.png");
        case ScRecord::GameSc2:
            return QStringLiteral(QT_STRINGIFY(SAILFISH_DATADIR) "/media/sc2.png");
        default:
            return QStringLiteral(QT_STRINGIFY(SAILFISH_DATADIR) "/media/random.png");
        }
    } else if (QStringLiteral("event_name") == key) {
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
                auto filePath = m_IconDir + q.value(0).toString();
                if (QFile::exists(filePath)) {
                    return filePath;
                }
            }
        }
    }

    return QStringLiteral("image://theme/icon-m-sailfish");
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
    static char const* const DropSql[] = {
        "DROP INDEX IF EXISTS vods_id",
        "DROP INDEX IF EXISTS vods_game",
        "DROP INDEX IF EXISTS vods_year",
        "DROP INDEX IF EXISTS vods_season",
        "DROP INDEX IF EXISTS vods_event_name",
        "DROP INDEX IF EXISTS vods_seen",

        "DROP VIEW IF EXISTS offline_vods",
        "DROP VIEW IF EXISTS url_share_vods",


        "DROP TABLE IF EXISTS vods",
        "DROP TABLE IF EXISTS vod_files",
        "DROP TABLE IF EXISTS vod_thumbnails ",
        "DROP TABLE IF EXISTS vod_url_share",
        "DROP TABLE IF EXISTS settings",
        "DROP TABLE IF EXISTS icons",
    };

    QSqlQuery q(m_Database);
    for (size_t i = 0; i < _countof(DropSql); ++i) {
        if (!q.prepare(DropSql[i]) || !q.exec()) {
            qCritical() << "failed to drop table" << q.lastError();
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
        QDir(m_MetaDataDir).removeRecursively();
        QDir().mkpath(m_MetaDataDir);
    }

    if (flags & CF_Thumbnails) {
        QDir(m_ThumbnailDir).removeRecursively();
        QDir().mkpath(m_ThumbnailDir);
    }

    if (flags & CF_Vods) {
        QDir(m_VodDir).removeRecursively();
        QDir().mkpath(m_VodDir);

        tryRaiseVodsChanged();

        emit vodsCleared();
    }

    if (flags & CF_Icons) {
        QDir(m_IconDir).removeRecursively();
        QDir().mkpath(m_IconDir);
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
ScVodDataManager::clearIcons() {
    QMutexLocker g(&m_Lock);

    RETURN_IF_ERROR;

    // delete files
    QDir(m_IconDir).removeRecursively();
    // create dir
    QDir().mkpath(m_IconDir);
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
        record.valid |= ScRecord::ValidStage;
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
                 ScRecord::ValidStage |
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
                       ScRecord::ValidStage |
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
    Stopwatch sw("_addVods", 10000);
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
                    "   match_number\n"
                    ") VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)\n");
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

        if (!q.exec()) {
            qCritical() << "failed to exec vod insert" << q.lastError();
            continue;
        }

        if (q.lastInsertId().isValid()) {
            ++count;
        } else {
            qDebug() << "duplicate record" << record;
        }

        if (!record.isValid(ScRecord::ValidGame)) {
            unknownGameRowids << qvariant_cast<qint64>(q.lastInsertId());
            unknownGameIndices << i;
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
        q.addBindValue(record.isValid(ScRecord::ValidSeason) ? record.season : 0);

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

void ScVodDataManager::onMetaDataDownloadCompleted(qint64 token, const VMVod& vod) {
    QMutexLocker g(&m_Lock);
    auto it = m_VodmanMetaDataRequests.find(token);
    if (it != m_VodmanMetaDataRequests.end()) {
        VodmanMetaDataRequest r = it.value();
        m_VodmanMetaDataRequests.erase(it);

        if (vod.isValid()) {
            // save vod meta data
            auto metaDataFilePath = m_MetaDataDir + QString::number(r.vod_url_share_id);
            QFile file(metaDataFilePath);
            if (file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
                {
                    QDataStream s(&file);
                    s << vod;
                }

                file.close(); // to flush data

                // send event so that the match page can try to get the thumbnails again
                {
                    QSqlQuery q(m_Database);

                    // update title, length from vod
                    {
                        static const QString sql = QStringLiteral(
                                    "UPDATE vod_url_share SET title=?, length=? WHERE id=?");
                        if (!q.prepare(sql)) {
                            qCritical() << "failed to prepare query" << q.lastError();
                            return;
                        }

                        q.addBindValue(vod.description().title());
                        q.addBindValue(vod.description().duration());
                        q.addBindValue(r.vod_url_share_id);

                        if (!q.exec()) {
                            qCritical() << "failed to exec query" << q.lastError();
                            return;
                        }
                    }

                    // emit metaDataAvailable
                    {
                        if (Q_UNLIKELY(!selectIdFromVodsWhereUrlShareIdEquals(q, r.vod_url_share_id))) {
                            return;
                        }

                        while (q.next()) {
                            auto rowid = qvariant_cast<qint64>(q.value(0));
                            qDebug() << "metaDataAvailable" << rowid;
                            emit metaDataAvailable(rowid, vod);
                        }
                    }
                }
            } else {
                // error
                qDebug() << "failed to open meta data file" << metaDataFilePath << "for writing";
            }
        } else {
            qDebug() << "token" << token << "invalid vod" << vod;
        }
    }
}

void ScVodDataManager::onFileDownloadCompleted(qint64 token, const VMVodFileDownload& download) {
    QMutexLocker g(&m_Lock);
    auto it = m_VodmanFileRequests.find(token);
    if (it != m_VodmanFileRequests.end()) {
        const VodmanFileRequest r = it.value();
        m_VodmanFileRequests.erase(it);
        emit vodDownloadsChanged();

        QSqlQuery q(m_Database);
        if (download.progress() >= 1 && download.error() == VMVodEnums::VM_ErrorNone) {
            updateVodDownloadStatus(r.vod_url_share_id, download);
        } else {

        }
    }
}

void ScVodDataManager::onFileDownloadChanged(qint64 token, const VMVodFileDownload& download) {
    QMutexLocker g(&m_Lock);
    auto it = m_VodmanFileRequests.find(token);
    if (it != m_VodmanFileRequests.end()) {
        const VodmanFileRequest& r = it.value();

        updateVodDownloadStatus(r.vod_url_share_id, download);
    }
}

void ScVodDataManager::updateVodDownloadStatus(
        qint64 urlShareId,
        const VMVodFileDownload& download) {
    QSqlQuery q(m_Database);

    static const QString UpdateSql = QStringLiteral(
                "UPDATE vod_files\n"
                "SET progress=?\n"
                "WHERE id IN\n"
                "   (SELECT vod_file_id from vod_url_share WHERE id=?)");

    if (!q.prepare(UpdateSql)) {
        qCritical() << "failed to prepare query" << q.lastError();
        return;
    }

    q.addBindValue(download.progress());
    q.addBindValue(urlShareId);

    if (!q.exec()) {
        qCritical() << "failed to exec query" << q.lastError();
        return;
    }

    if (Q_UNLIKELY(!selectIdFromVodsWhereUrlShareIdEquals(q, urlShareId))) {
        return;
    }

    while (q.next()) {
        auto vodId = qvariant_cast<qint64>(q.value(0));
        emit vodAvailable(
                    vodId,
                    download.filePath(),
                    download.progress(),
                    download.fileSize(),
                    download.format().width(),
                    download.format().height(),
                    download.format().id());
    }
}

void
ScVodDataManager::onDownloadFailed(qint64 token, int serviceErrorCode) {
    QMutexLocker g(&m_Lock);

    qint64 urlShareId = -1;
    auto metaData = false;

    auto it = m_VodmanFileRequests.find(token);
    if (it != m_VodmanFileRequests.end()) {
        VodmanFileRequest r = it.value();
        m_VodmanFileRequests.erase(it);
        emit vodDownloadsChanged();

        urlShareId = r.vod_url_share_id;
        metaData = false;
    } else {
        auto it2 = m_VodmanMetaDataRequests.find(token);
        if (it2 != m_VodmanMetaDataRequests.end()) {
            VodmanMetaDataRequest r = it2.value();
            m_VodmanMetaDataRequests.erase(it2);

            urlShareId = r.vod_url_share_id;
            metaData = true;
        }
    }

    if (urlShareId >= 0) {
        QSqlQuery q(m_Database);
        if (Q_UNLIKELY(!selectIdFromVodsWhereUrlShareIdEquals(q, urlShareId))) {
            return;
        }

        while (q.next()) {
            auto rowid = qvariant_cast<qint64>(q.value(0));
            if (metaData) {
                emit metaDataDownloadFailed(rowid, serviceErrorCode);
            } else {
                emit vodDownloadFailed(rowid, serviceErrorCode);
            }
        }
    }
}


void
ScVodDataManager::fetchMetaData(qint64 rowid) {
    return fetchMetaData(rowid, true);
}

void
ScVodDataManager::fetchMetaDataFromCache(qint64 rowid) {
    return fetchMetaData(rowid, false);
}

void
ScVodDataManager::fetchMetaData(qint64 rowid, bool download) {
    RETURN_IF_ERROR;

    Stopwatch sw("fetchMetaData", 10);
    QMutexLocker g(&m_Lock);

    QSqlQuery q(m_Database);

    static const QString sql = QStringLiteral(
                "SELECT u.url, u.id FROM vods AS v INNER JOIN vod_url_share "
                "AS u ON v.vod_url_share_id=u.id WHERE v.id=?");
    if (!q.prepare(sql)) {
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

    const auto vodUrl = q.value(0).toString();
    const auto urlShareId = qvariant_cast<qint64>(q.value(1));

    const auto metaDataFilePath = m_MetaDataDir + QString::number(urlShareId);
    QFileInfo fi(metaDataFilePath);
    if (fi.exists()) {
        QFile file(metaDataFilePath);
        if (fi.created().addSecs(3600) >= QDateTime::currentDateTime()) {
            if (file.open(QIODevice::ReadOnly)) {
                VMVod vod;
                {
                    QDataStream s(&file);
                    s >> vod;
                }

                if (vod.isValid()) {
                    emit metaDataAvailable(rowid, vod);
                } else {
                    // read invalid vod, try again
                    file.close();
                    file.remove();
                    if (download) {
                        emit fetchingMetaData(rowid);
                        fetchMetaData(urlShareId, vodUrl);
                    }
                }
            } else {
                file.remove();
                if (download) {
                    emit fetchingMetaData(rowid);
                    fetchMetaData(urlShareId, vodUrl);
                }
            }
        } else {
            // vod links probably expired
            file.remove();
            if (download) {
                emit fetchingMetaData(rowid);
                fetchMetaData(urlShareId, vodUrl);
            }
        }
    } else {
        if (download) {
            emit fetchingMetaData(rowid);
            fetchMetaData(urlShareId, vodUrl);
        }
    }
}

void
ScVodDataManager::fetchThumbnail(qint64 rowid) {
    fetchThumbnail(rowid, true);
}

void
ScVodDataManager::fetchThumbnailFromCache(qint64 rowid) {
    fetchThumbnail(rowid, false);
}

void ScVodDataManager::fetchThumbnail(qint64 rowid, bool download) {
    RETURN_IF_ERROR;

    Stopwatch sw("fetchThumbnail", 50);
    QMutexLocker g(&m_Lock);

    QSqlQuery q(m_Database);

    static const QString sql = QStringLiteral(
                "SELECT v.thumbnail_id, u.url, u.id FROM vods AS v INNER JOIN vod_url_share "
                "AS u ON v.vod_url_share_id=u.id WHERE v.id=?");
    if (!q.prepare(sql)) {
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

    auto thumbnailId = q.value(0).isNull() ? 0 : qvariant_cast<qint64>(q.value(0));
    const auto vodUrl = q.value(1).toString();
    const auto vodUrlShareId = qvariant_cast<qint64>(q.value(2));
    if (thumbnailId) {
        // entry in table
        static const QString sql = QStringLiteral("SELECT file_name, url FROM vod_thumbnails WHERE id=?");
        if (!q.prepare(sql)) {
            qCritical() << "failed to prepare query" << q.lastError();
            return;
        }

        q.addBindValue(thumbnailId);

        if (!q.exec()) {
            qCritical() << "failed to exec query" << q.lastError();
            return;
        }

        if (!q.next()) {
            qDebug() << "ouch not data for thumbnail id" << thumbnailId;
            return;
        }

        auto fileName = q.value(0).toString();
        auto thumbNailFilePath = m_ThumbnailDir + fileName;
        if (QFileInfo::exists(thumbNailFilePath)) {
            emit thumbnailAvailable(rowid, thumbNailFilePath);
        } else {
            if (download) {
                auto thumbnailUrl = q.value(1).toString();
                fetchThumbnailFromUrl(thumbnailId, thumbnailUrl);
            }
        }
    } else { // thumb nail id not set
        if (download) {
            auto metaDataFilePath = m_MetaDataDir + QString::number(vodUrlShareId);
            if (QFileInfo::exists(metaDataFilePath)) {
                QFile file(metaDataFilePath);
                if (file.open(QIODevice::ReadOnly)) {
                    VMVod vod;
                    {
                        QDataStream s(&file);
                        s >> vod;
                    }

                    if (vod.isValid()) {
                        auto thumnailUrl = vod.description().thumbnailUrl();
                        if (thumnailUrl.isEmpty()) {
                            return;
                        }

                        static const QString sql = QStringLiteral("SELECT id FROM vod_thumbnails WHERE url=?");
                        if (!q.prepare(sql)) {
                            qCritical() << "failed to prepare query" << q.lastError();
                            return;
                        }

                        q.addBindValue(thumnailUrl);

                        if (!q.exec()) {
                            qCritical() << "failed to exec query" << q.lastError();
                            return;
                        }

                        if (q.next()) { // url has been retrieved, set thumbnail id in vods
                            thumbnailId = qvariant_cast<qint64>(q.value(0));

                            static const QString sql = QStringLiteral("UPDATE vods SET thumbnail_id=? WHERE id=?");
                            if (!q.prepare(sql)) {
                                qCritical() << "failed to prepare query" << q.lastError();
                                return;
                            }

                            q.addBindValue(thumbnailId);
                            q.addBindValue(rowid);

                            if (!q.exec()) {
                                qCritical() << "failed to exec query" << q.lastError();
                                return;
                            }
                        } else {
                            auto lastDot = thumnailUrl.lastIndexOf(QChar('.'));
                            auto extension = thumnailUrl.mid(lastDot);
                            QTemporaryFile tempFile(m_ThumbnailDir + QStringLiteral("XXXXXX") + extension);
                            tempFile.setAutoRemove(false);
                            if (tempFile.open()) {
                                auto thumbNailFilePath = tempFile.fileName();
                                auto tempFileName = QFileInfo(thumbNailFilePath).fileName();

                                // insert row for thumbnail
                                static const QString sql = QStringLiteral("INSERT INTO vod_thumbnails (url, file_name) VALUES (?, ?)");
                                if (!q.prepare(sql)) {
                                    qCritical() << "failed to prepare query" << q.lastError();
                                    return;
                                }

                                q.addBindValue(thumnailUrl);
                                q.addBindValue(tempFileName);

                                if (!q.exec()) {
                                    qCritical() << "failed to exec query" << q.lastError();
                                    return;
                                }

                                thumbnailId = qvariant_cast<qint64>(q.lastInsertId());

                                // update vod row
                                static const QString sql2 = QStringLiteral("UPDATE vods SET thumbnail_id=? WHERE id=?");
                                if (!q.prepare(sql2)) {
                                    qCritical() << "failed to prepare query" << q.lastError();
                                    return;
                                }

                                q.addBindValue(thumbnailId);
                                q.addBindValue(rowid);

                                if (!q.exec()) {
                                    qCritical() << "failed to exec query" << q.lastError();
                                    return;
                                }

                                fetchThumbnailFromUrl(thumbnailId, thumnailUrl);
                            } else {
                                qWarning() << "failed to allocate temporary file for thumbnail";
                            }
                        }
                    } else {
                        // read invalid vod, try again
                        file.close();
                        file.remove();
                        emit fetchingMetaData(rowid);
                        fetchMetaData(vodUrlShareId, vodUrl);
                    }
                } else {
                    file.remove();
                    emit fetchingMetaData(rowid);
                    fetchMetaData(vodUrlShareId, vodUrl);
                }
            } else {
                emit fetchingMetaData(rowid);
                fetchMetaData(vodUrlShareId, vodUrl);
            }
        }
    }
}

void ScVodDataManager::fetchMetaData(qint64 urlShareId, const QString& url) {
    for (auto& value : m_VodmanMetaDataRequests) {
        if (value.vod_url_share_id == urlShareId) {
            return; // already underway
        }
    }

    VodmanMetaDataRequest r;
    r.vod_url_share_id = urlShareId;
    auto token = m_Vodman->newToken();
    m_VodmanMetaDataRequests.insert(token, r);
    m_Vodman->startFetchMetaData(token, url);
}

void ScVodDataManager::fetchThumbnailFromUrl(qint64 rowid, const QString& url) {
    foreach (const auto& value, m_ThumbnailRequests.values()) {
        if (value.rowid == rowid) {
            return; // already underway
        }
    }

    ThumbnailRequest r;
    r.rowid = rowid;
    m_ThumbnailRequests.insert(m_Manager->get(Sc::makeRequest(url)), r);
}

void ScVodDataManager::fetchVod(qint64 rowid, int formatIndex, bool implicitlyStarted) {
    RETURN_IF_ERROR;

    if (formatIndex < 0) {
        qWarning() << "invalid format index, not fetching vod" << formatIndex;
        return;
    }


    QMutexLocker g(&m_Lock);

    QSqlQuery q(m_Database);

    static const QString sql = QStringLiteral(
                "SELECT\n"
                "    file_name,\n"
                "    progress,\n"
                "    format,"
                "    url,\n"
                "    vod_url_share_id\n"
                "FROM offline_vods\n"
                "WHERE id=?\n"
                );
    if (!q.prepare(sql)) {
        qCritical() << "failed to prepare query" << q.lastError();
        return;
    }


    q.addBindValue(rowid);

    if (!q.exec()) {
        qCritical() << "failed to exec query" << q.lastError();
        return;
    }

    QString vodFilePath;
    QString vodUrl;
    qint64 urlShareId = -1;
    QString formatId;

    float progress = 0;
    if (q.next()) {
        vodFilePath = m_VodDir + q.value(0).toString();
        progress = q.value(1).toFloat();
        formatId = q.value(2).toString();
        vodUrl = q.value(3).toString();
        urlShareId = qvariant_cast<qint64>(q.value(4));
    } else {
        static const QString sql = QStringLiteral(
                    "SELECT\n"
                    "   url,\n"
                    "   vod_url_share_id\n"
                    "FROM vods v\n"
                    "INNER JOIN vod_url_share u ON v.vod_url_share_id=u.id\n"
                    "WHERE v.id=?\n"
                    );
        if (!q.prepare(sql)) {
            qCritical() << "failed to prepare query" << q.lastError();
            return;
        }

        q.addBindValue(rowid);

        if (!q.exec()) {
            qCritical() << "failed to exec query" << q.lastError();
            return;
        }

        if (!q.next()) {
            // row gone
            return;
        }

        vodUrl = q.value(0).toString();
        urlShareId = qvariant_cast<qint64>(q.value(1));
    }

    for (auto& value : m_VodmanFileRequests) {
        if (value.vod_url_share_id == urlShareId) {
            if (value.implicitlyStarted && !implicitlyStarted) {
                value.implicitlyStarted = false;
            }
            return; // already underway
        }
    }

    auto metaDataFilePath = m_MetaDataDir + QString::number(urlShareId);
    if (QFileInfo::exists(metaDataFilePath)) {
        QFile file(metaDataFilePath);
        if (file.open(QIODevice::ReadOnly)) {
            VMVod vod;
            {
                QDataStream s(&file);
                s >> vod;
            }

            if (vod.isValid()) {
                if (!formatId.isEmpty()) {
                    auto formats = vod._formats();
                    for (auto i = 0; i < formats.size(); ++i) {
                        const auto& format = formats[i];
                        if (format.id() == formatId) {
                            qDebug() << "using previous selected format at index" << i << "id" << formatId;
                            formatIndex = i;
                            break;
                        }
                    }
                }

                if (formatIndex >= vod._formats().size()) {
                    qWarning() << "invalid format index (>=), not fetching vod" << formatIndex;
                    return;
                }

                auto format = vod._formats()[formatIndex];

                if (vodFilePath.isEmpty()) {
                    QTemporaryFile tempFile(m_VodDir + QStringLiteral("XXXXXX.") + format.fileExtension());
                    if (tempFile.open()) {
                        vodFilePath = tempFile.fileName();
                        auto tempFileName = QFileInfo(vodFilePath).fileName();
                        static const QString sql = QStringLiteral(
                                    "INSERT INTO vod_files (file_name, format, width, height, progress) "
                                    "VALUES (?, ?, ?, ?, ?)");
                        if (!q.prepare(sql)) {
                            qCritical() << "failed to prepare query" << q.lastError();
                            return;
                        }

                        q.addBindValue(tempFileName);
                        q.addBindValue(format.id());
                        q.addBindValue(format.width());
                        q.addBindValue(format.height());
                        q.addBindValue(0);

                        if (!q.exec()) {
                            qCritical() << "failed to exec query" << q.lastError();
                            return;
                        }

                        auto vodFileId = qvariant_cast<qint64>(q.lastInsertId());

                        static const QString sql2 = QStringLiteral("UPDATE vod_url_share SET vod_file_id=? WHERE id=?");
                        if (!q.prepare(sql2)) {
                            qCritical() << "failed to prepare query" << q.lastError();
                            return;
                        }

                        q.addBindValue(vodFileId);
                        q.addBindValue(urlShareId);

                        if (!q.exec()) {
                            qCritical() << "failed to exec query" << q.lastError();
                            return;
                        }

                        // all good, keep file
                        tempFile.setAutoRemove(false);
                    } else {
                        qWarning() << "failed to create temporary file for vod" << rowid;
                        return;
                    }
                } else {
                    QFileInfo fi(vodFilePath);
                    if (fi.exists()) {
                        if (progress >= 1) {
                            emit vodAvailable(
                                        rowid,
                                        vodFilePath,
                                        progress,
                                        fi.size(),
                                        format.width(),
                                        format.height(),
                                        formatId);
                            return;
                        }
                    }
                }

                VodmanFileRequest r;
                r.token = m_Vodman->newToken();
                r.vod_url_share_id = urlShareId;
                r.implicitlyStarted = implicitlyStarted;
                m_VodmanFileRequests.insert(r.token, r);
                m_Vodman->startFetchFile(r.token, vod, formatIndex, vodFilePath);
                emit vodDownloadsChanged();

            } else {
                // read invalid vod, try again
                file.close();
                file.remove();
                fetchMetaData(urlShareId, vodUrl);
            }
        } else {
            file.remove();
            fetchMetaData(urlShareId, vodUrl);
        }
    } else {
        fetchMetaData(urlShareId, vodUrl);
    }
}



void
ScVodDataManager::cancelFetchVod(qint64 rowid) {
    RETURN_IF_ERROR;


    QMutexLocker g(&m_Lock);

    QSqlQuery q(m_Database);

    static const QString selectUrlShareId = QStringLiteral("SELECT vod_url_share_id FROM vods WHERE id=?");
    if (!q.prepare(selectUrlShareId)) {
        qCritical() << "failed to prepare query" << q.lastError();
        return;
    }


    q.addBindValue(rowid);

    if (!q.exec()) {
        qCritical() << "failed to exec query" << q.lastError();
        return;
    }

    while (q.next()) {
        auto vodUrlShareId = qvariant_cast<qint64>(q.value(0));
        auto beg = m_VodmanFileRequests.begin();
        auto end = m_VodmanFileRequests.end();
        for (auto it = beg; it != end; ) {
            if (it.value().vod_url_share_id == vodUrlShareId) {
                m_Vodman->cancel(it.key());
                m_VodmanFileRequests.erase(it++);
                qDebug() << "cancel vod file request for url share id" << vodUrlShareId;
            } else {
                ++it;
            }
        }
    }

    emit vodDownloadsChanged();

    if (!q.prepare(QStringLiteral("SELECT id FROM vods WHERE vod_url_share_id in (%1)").arg(selectUrlShareId))) {
        qCritical() << "failed to prepare query" << q.lastError();
        return;
    }


    q.addBindValue(rowid);

    if (!q.exec()) {
        qCritical() << "failed to exec query" << q.lastError();
        return;
    }

    while (q.next()) {
        auto vodId = qvariant_cast<qint64>(q.value(0));
        emit vodDownloadCanceled(vodId);
    }
}


void
ScVodDataManager::cancelFetchMetaData(qint64 rowid) {
    RETURN_IF_ERROR;

    QMutexLocker g(&m_Lock);

    qDebug() << "cancel meta data fetch for" << rowid;

    QSqlQuery q(m_Database);

    static const QString sql = QStringLiteral(
                "SELECT vod_url_share_id FROM vods WHERE id=?");

    if (!q.prepare(sql)) {
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

    auto beg = m_VodmanMetaDataRequests.cbegin();
    auto end = m_VodmanMetaDataRequests.cend();
    for (auto it = beg; it != end; ++it) {
        if (it.value().vod_url_share_id == urlShareId) {
            auto token = it.key();
            m_VodmanMetaDataRequests.remove(token);
            m_Vodman->cancel(token);
            return;
        }
    }
}


void
ScVodDataManager::vacuum() {
    RETURN_IF_ERROR;

    QMutexLocker g(&m_Lock);

    QList<qint64> thumbnailRowsToDelete;
    QList<qint64> vodFileRowsToDelete;

    QSqlQuery q(m_Database);

    if (!q.exec("SELECT id, file_name FROM vod_thumbnails")) {
        qCritical() << "failed to prepare query to fetch vod thumbnails" << q.lastError();
        return;
    }

    while (q.next()) {
        auto path = m_ThumbnailDir + q.value(1).toString();
        if (!QFileInfo::exists(path)) {
             thumbnailRowsToDelete << qvariant_cast<qint64>(q.value(0));
        }
    }

    if (!q.exec("SELECT id, file_name FROM vod_files")) {
        qCritical() << "failed to prepare query to fetch vod file names" << q.lastError();
        return;
    }

    while (q.next()) {
        auto path = m_VodDir + q.value(1).toString();
        if (!QFileInfo::exists(path)) {
             vodFileRowsToDelete << qvariant_cast<qint64>(q.value(0));
        }
    }

    if (!q.exec("BEGIN TRANSACTION")) {
        qCritical() << "failed to start transaction" << q.lastError();
        return;
    }

    for (const auto& id : thumbnailRowsToDelete) {
        if (!q.prepare("DELETE FROM vod_thumbnails WHERE id=?")) {
            qCritical() << "failed to prepare query to delete row" << q.lastError();
            goto Rollback;
        }

        q.addBindValue(id);

        if (!q.exec()) {
            qCritical() << "failed to delete row" << q.lastError();
            goto Rollback;
        }
    }

    for (const auto& id : vodFileRowsToDelete) {
        if (!q.prepare("DELETE FROM vod_files WHERE id=?")) {
            qCritical() << "failed to prepare query to delete row" << q.lastError();
            goto Rollback;
        }

        q.addBindValue(id);

        if (!q.exec()) {
            qCritical() << "failed to delete row" << q.lastError();
            goto Rollback;
        }
    }

    if (!q.exec("COMMIT TRANSACTION")) {
        qCritical() << "failed to commit transaction" << q.lastError();
        return;
    }

Exit:
    return;

Rollback:
    q.exec("ROLLBACK");
    goto Exit;
}

void
ScVodDataManager::addThumbnail(qint64 rowid, const QByteArray& bytes) {

    QSqlQuery q(m_Database);

    if (!q.prepare(QStringLiteral("SELECT file_name FROM vod_thumbnails WHERE id=?"))) {
        qCritical() << "failed to prepare query" << q.lastError();
        return;
    }

    q.addBindValue(rowid);

    if (!q.exec()) {
        qCritical() << "failed to exec query" << q.lastError();
        return;
    }

    if (!q.next()) {
        qDebug() << "thumbnail rowid" << rowid << "gone";
        return;
    }

    auto thumbnailFileName = q.value(0).toString();

    // this nonsense is necessary b/c QImage doesn't use the
    // mime type but the extension
    auto lastDot = thumbnailFileName.lastIndexOf(QChar('.'));
    auto extension = thumbnailFileName.mid(lastDot + 1);
    auto mimeData = QMimeDatabase().mimeTypeForData(bytes);
    if (mimeData.isValid() && !mimeData.suffixes().contains(extension)) {
        auto newFileName = thumbnailFileName.left(lastDot) + QStringLiteral(".") + mimeData.preferredSuffix();
        qDebug() << "changing" << thumbnailFileName << "to" << newFileName;
        if (!q.prepare(QStringLiteral("UPDATE vod_thumbnails SET file_name=? WHERE id=?"))) {
            qCritical() << "failed to prepare query" << q.lastError();
            return;
        }

        q.addBindValue(newFileName);
        q.addBindValue(rowid);

        if (!q.exec()) {
            qCritical() << "failed to exec query" << q.lastError();
            return;
        }

        // remove old thumbnail file if it exists
        QFile::remove(m_ThumbnailDir + thumbnailFileName);

        thumbnailFileName = newFileName;
    }

    auto thumbnailFilePath = m_ThumbnailDir + thumbnailFileName;
    QFile file(thumbnailFilePath);
    if (file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        if (bytes.size() == file.write(bytes)) {
            file.close();
            // all is well


            // notify thumbnail is available
            if (!q.prepare(QStringLiteral("SELECT id FROM vods WHERE thumbnail_id=?"))) {
                qCritical() << "failed to prepare query" << q.lastError();
                return;
            }

            q.addBindValue(rowid);

            if (!q.exec()) {
                qCritical() << "failed to exec query" << q.lastError();
                return;
            }

            while (q.next()) {
                auto vodRowId = qvariant_cast<qint64>(q.value(0));
                qDebug() << "thumbnailAvailable" << vodRowId << thumbnailFilePath;
                emit thumbnailAvailable(vodRowId, thumbnailFilePath);
            }
        } else {
            // fix me, no space left
            qDebug() << "no space left on device";
        }
    } else {
        // fix me
        qDebug() << "failed to open" << thumbnailFilePath << "for writing";
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
ScVodDataManager::deleteVod(qint64 rowid) {
    deleteVodsWhere(QStringLiteral("WHERE id=%1").arg(rowid));
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
        auto path = m_ThumbnailDir + fileName;
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
    auto metaDataFilePath = m_MetaDataDir + QString::number(urlShareId);
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

QString
ScVodDataManager::title(qint64 rowid) {
    RETURN_IF_ERROR;

    QMutexLocker g(&m_Lock);

    QSqlQuery q(m_Database);

    if (!q.prepare(QStringLiteral(
"SELECT u.title FROM vods AS v INNER JOIN vod_url_share "
"AS u ON v.vod_url_share_id=u.id WHERE v.id=?"))) {
        qCritical() << "failed to prepare query" << q.lastError();
        return QString();
    }

    q.addBindValue(rowid);

    if (!q.exec()) {
        qCritical() << "failed to exec query" << q.lastError();
        return QString();
    }

    if (q.next()) {
        return q.value(0).toString();
    }

    return QString();
}

void
ScVodDataManager::queryVodFiles(qint64 rowid) {
    RETURN_IF_ERROR;

    QMutexLocker g(&m_Lock);

    QSqlQuery q(m_Database);

    if (!q.prepare(QStringLiteral(
                      "SELECT\n"
                      "    file_name,\n"
                      "    progress,\n"
                      "    width,\n"
                      "    height,\n"
                      "    format\n"
                      "FROM offline_vods\n"
                      "WHERE id=?\n"
                      ))) {
        qCritical() << "failed to prepare query" << q.lastError();
        return;
    }


    q.addBindValue(rowid);

    if (!q.exec()) {
        qCritical() << "failed to exec query" << q.lastError();
        return;
    }

    QFileInfo fi;
    while (q.next()) {
        auto filePath = m_VodDir + q.value(0).toString();

        fi.setFile(filePath);
        if (fi.exists()) {
            auto progress = q.value(1).toFloat();
            auto width = q.value(2).toInt();
            auto height = q.value(3).toInt();
            auto format = q.value(4).toString();

            emit vodAvailable(rowid, filePath, progress, fi.size(), width, height, format);
        }
    }
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


        QTemporaryFile tempFile(m_ThumbnailDir + QStringLiteral("XXXXXX") + extension);
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

    return deleteVodsWhere(w2);
}

int
ScVodDataManager::deleteVodsWhere(const QString& where) {
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
        if (QFile::remove(m_VodDir + fileName)) {
            qDebug() << "removed" << m_VodDir + fileName;
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

bool
ScVodDataManager::implicitlyStartedVodFetch(qint64 rowid) const {
    RETURN_IF_ERROR;

    QMutexLocker g(&m_Lock);

    QSqlQuery q(m_Database);

    if (!q.prepare(QStringLiteral("SELECT vod_url_share_id FROM vods WHERE id=?"))) {
        qCritical() << "failed to prepare query" << q.lastError();
        return false;
    }

    q.addBindValue(rowid);

    if (!q.exec()) {
        qCritical() << "failed to exec query" << q.lastError();
        return false;
    }

    if (!q.next()) {
        qDebug() << "rowid" << rowid << "gone";
        return false;
    }

    const auto urlShareId = qvariant_cast<qint64>(q.value(0));

    auto beg = m_VodmanFileRequests.cbegin();
    auto end = m_VodmanFileRequests.cend();
    for (auto it = beg; it != end; ++it) {
        const VodmanFileRequest& r = it.value();
        if (r.vod_url_share_id == urlShareId) {
            return r.implicitlyStarted;
        }
    }

    return false;
}

int
ScVodDataManager::vodDownloads() const {
    QMutexLocker g(&m_Lock);
    return m_VodmanFileRequests.size();
}

QVariantList
ScVodDataManager::vodsBeingDownloaded() const {
    QMutexLocker g(&m_Lock);

    QSqlQuery q(m_Database);
    QVariantList result;

    auto beg = m_VodmanFileRequests.cbegin();
    auto end = m_VodmanFileRequests.cend();
    for (auto it = beg; it != end; ++it) {
        const VodmanFileRequest& r = it.value();

        if (Q_UNLIKELY(!selectIdFromVodsWhereUrlShareIdEquals(q, r.vod_url_share_id))) {
            return result;
        }

        while (q.next()) {
            auto vodId = qvariant_cast<qint64>(q.value(0));
            result << vodId;
        }
    }

    return result;
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
        "DROP TABLE settings\n",
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

    qInfo("Update database v1 to v2 completed successfully\n");
}

void
ScVodDataManager::updateSql2(QSqlQuery& q) {
    qInfo("Begin update database v2 to v3\n");

    static char const* const Sql1[] = {
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
        "DROP VIEW offline_vods",
        "DROP TABLE vod_file_ref",
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
    setError(Error_CouldntCreateSqlTables);
    goto Exit;
}





void
ScVodDataManager::updateSql3(QSqlQuery& q) {
    qInfo("Begin update database v3 to v4\n");

    static char const* const Sql1[] = {
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
        auto metaDataFilePath = m_MetaDataDir + QString::number(urlShareId);
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
        "DROP VIEW offline_vods",
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
    setError(Error_CouldntCreateSqlTables);
    goto Exit;
}

int
ScVodDataManager::sqlPatchLevel() const
{
    return getPersistedValue(s_SqlPatchLevelKey, s_SqlPatchLevelDefault).toInt();
}

int
ScVodDataManager::getVodEndOffset(qint64 rowid, int offset, int vodLengthS) const
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
    return getPersistedValue(QStringLiteral("data-dir"), QStandardPaths::writableLocation(QStandardPaths::CacheLocation));
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
    connect(m_DataDirectoryMover, &DataDirectoryMover::dataDirectoryChanging, this, &ScVodDataManager::onDataDirectoryChanging);

//    QDir d(QDir(targetDirectory).canonicalPath());
//    if (!d.exists()) {
//        if (!d.mkpath(d.absolutePath())) {
//            qWarning() << "failed to create" << d.absolutePath();
//            return;
//        }
//    }

//    const auto currentDirectory = dataDirectory();
//    auto work = [currentDirectory, targetDirectory]() {

//    };



//    QStringList args;
//    args << QStringLiteral("-L") << QStringLiteral("-c") << QStringLiteral("%m");

//    QByteArray currentMountPoint, targetMountPoint;

//    auto fun = [&](const QString& dir, QByteArray* mountPoint) {
//        //    process.setProcessChannelMode(QProcess::MergedChannels);
//            QProcess process;
//            args << dir;
//            process.start(QStringLiteral("stat"), args, QIODevice::ReadOnly);

//            // Wait for it to start
//            if (!process.waitForStarted()) {
//                emit dataDirectoryChanging(DDCT_Finished, dir, 0, Error_ProcessOutOfResources, QStringLiteral("Failed to start process"));
//                return false;
//            }

//            if (!process.waitForFinished()) {
//                process.kill();
//                emit dataDirectoryChanging(DDCT_Finished, dir, 0, Error_ProcessTimeout, QStringLiteral("Timeout after 30 s"));
//                return false;
//            }

//            if (process.exitStatus() != QProcess::NormalExit) {
//                emit dataDirectoryChanging(DDCT_Finished, dir, 0, Error_ProcessCrashed, QString());
//                return false;
//            }

//            if (process.exitCode() != 0) {
//                process.setReadChannel(QProcess::StandardError);
//                emit dataDirectoryChanging(DDCT_Finished, dir, 0, Error_ProcessFailed, QString::fromLocal8Bit(process.readAll()));
//                return false;
//            }

//            *mountPoint = process.readAll();
//            if (mountPoint->isEmpty() || (*mountPoint)[0] != '/') {
//                emit dataDirectoryChanging(
//                            DDCT_Finished,
//                            dataDirectory(),
//                            0,
//                            Error_ProcessFailed,
//                            QStringLiteral("'stat %1' output: '%2'").arg(args.join(" "), QString::fromLocal8Bit(*mountPoint)));
//                return false;
//            }

//            args.pop_back();
//            return true;
//    };

//    if (!fun(dataDirectory(), &currentMountPoint)) {
//        return;
//    }

//    if (!fun(targetDirectory, &targetMountPoint)) {
//        return;
//    }

//    if (targetMountPoint == currentMountPoint) {
//        // move dirs
//    } else {
//        // rsync dirs
//    }
}

void
ScVodDataManager::cancelMoveDataDirectory() {
    QMutexLocker g(&m_Lock);
    if (m_DataDirectoryMover) {
        m_DataDirectoryMover->cancel();
    }
}

void
ScVodDataManager::onDataDirectoryChanging(int changeType, QString path, float progress, int error, QString errorDescription) {
    QMutexLocker g(&m_Lock);
    if (DDCT_Finished == changeType) {
        m_DataDirectoryMover = nullptr;
        resumeVodsChangedEvents();
    }

    emit dataDirectoryChanging(changeType, path, progress, error, errorDescription);
}
