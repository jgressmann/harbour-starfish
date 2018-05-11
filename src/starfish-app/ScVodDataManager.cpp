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

#ifndef _countof
#   define _countof(x) (sizeof(x)/sizeof((x)[0]))
#endif

#define RETURN_IF_ERROR \
    do { \
        if (m_Status == Status_Error) { \
            qWarning() << "Error status, return"; \
        } \
    } while (0)



namespace  {

const QString s_IconsUrl = QStringLiteral("https://www.dropbox.com/s/p2640l82i3u1zkg/icons.json.gz?dl=1");
const QString s_ClassifierUrl = QStringLiteral("https://www.dropbox.com/s/3cckgyzlba8kev9/classifier.json.gz?dl=1");


} // anon

ScVodDataManager::~ScVodDataManager() {
}

ScVodDataManager::ScVodDataManager(QObject *parent)
    : QObject(parent)
    , m_Lock(QMutex::Recursive) {

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
    m_Status = Status_Ready;
    m_Error = Error_None;
    m_SuspendedVodsChangedEventCount = 0;
    m_ClassfierRequest = nullptr;


    const auto databaseDir = QStandardPaths::writableLocation(QStandardPaths::DataLocation);

    QDir d;
    d.mkpath(m_MetaDataDir);
    d.mkpath(m_ThumbnailDir);
    d.mkpath(m_VodDir);
    d.mkpath(m_IconDir);
    d.mkpath(databaseDir);

    auto databaseFilePath = databaseDir + "/vods.sqlite";
    m_Database = QSqlDatabase::addDatabase("QSQLITE", databaseFilePath);
    m_Database.setDatabaseName(databaseFilePath);
    if (m_Database.open()) {
        qInfo("Opened database %s\n", qPrintable(databaseFilePath));
        createTablesIfNecessary();
    } else {
        qCritical("Could not open database '%s'. Error: %s\n", qPrintable(databaseFilePath), qPrintable(m_Database.lastError().text()));
        m_Status = Status_Error;
    }


    // icons
    const auto iconsFileDestinationPath = databaseDir + QStringLiteral("/icons.json.gz");
    if (!QFile::exists(iconsFileDestinationPath)) {
        auto src = QStringLiteral(QT_STRINGIFY(SAILFISH_DATADIR) "/icons.json.gz");
        if (!QFile::copy(src, iconsFileDestinationPath)) {
            qCritical() << "could not copy" << src << "to" << iconsFileDestinationPath;
        }
    }

    auto result = m_Icons.load(iconsFileDestinationPath);
    Q_ASSERT(result);
    (void)result;

    // classifier
    const auto classifierFileDestinationPath = databaseDir + QStringLiteral("/classifier.json.gz");
    if (!QFile::exists(classifierFileDestinationPath)) {
        auto src = QStringLiteral(QT_STRINGIFY(SAILFISH_DATADIR) "/classifier.json.gz");
        if (!QFile::copy(src, classifierFileDestinationPath)) {
            qCritical() << "could not copy" << src << "to" << iconsFileDestinationPath;
        }
    }

    result = m_Classifier.load(classifierFileDestinationPath);
    Q_ASSERT(result);
    (void)result;

    fetchIcons();
    fetchClassifier();
}

void
ScVodDataManager::requestFinished(QNetworkReply* reply) {
    QMutexLocker guard(&m_Lock);
    reply->deleteLater();
    auto it = m_ThumbnailRequests.find(reply);
    if (it != m_ThumbnailRequests.end()) {
        auto r = it.value();
        m_ThumbnailRequests.erase(it);
        thumbnailRequestFinished(reply, r);
    } else {
        auto it2 = m_IconRequests.find(reply);
        if (it2 != m_IconRequests.end()) {
            auto r = it2.value();
            m_IconRequests.erase(it2);
            iconRequestFinished(reply, r);
        } else {
            if (m_ClassfierRequest == reply) {
                m_ClassfierRequest = nullptr;

                switch (reply->error()) {
                case QNetworkReply::OperationCanceledError:
                    break;
                case QNetworkReply::NoError: {
                    // Get the http status code
                    int v = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
                    if (v >= 200 && v < 300) {// Success
                        // Here we got the final reply
                        auto bytes = reply->readAll();
                        QFile file(QStandardPaths::writableLocation(QStandardPaths::DataLocation) + QStringLiteral("/classifier.json.gz"));
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
            }
        }
    }
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
                QFile file(QStandardPaths::writableLocation(QStandardPaths::DataLocation) + QStringLiteral("/icons.json.gz"));
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

                                q.addBindValue(r.url);

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
ScVodDataManager::createTablesIfNecessary() {
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
        "    url TEXT NOT NULL,\n"
        "    video_id TEXT,\n"
        "    title TEXT\n"
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
        "    thumbnail_id INTEGER REFERENCES vod_thumbnails(id) ON DELETE SET NULL,\n"
        "    FOREIGN KEY(vod_url_share_id) REFERENCES vod_url_share(id) ON DELETE CASCADE\n"
        ")\n",

        "CREATE TABLE IF NOT EXISTS vod_file_ref (\n"
        "    vod_id INTEGER,\n"
        "    vod_file_id INTEGER,\n"
        "    FOREIGN KEY(vod_id) REFERENCES vods(id) ON DELETE CASCADE,\n"
        "    FOREIGN KEY(vod_file_id) REFERENCES vod_files(id) ON DELETE CASCADE\n"
        ")\n",

        "CREATE TABLE IF NOT EXISTS settings (\n"
        "    key TEXT NOT NULL,\n"
        "    value TEXT\n"
        ")\n",

        "CREATE TABLE IF NOT EXISTS icons (\n"
        "    url TEXT NOT NULL,\n"
        "    file_name TEXT NOT NULL\n"
        ")\n",

        "CREATE INDEX IF NOT EXISTS vods_id ON vods (id)\n",
        "CREATE INDEX IF NOT EXISTS vods_game ON vods (game)\n",
        "CREATE INDEX IF NOT EXISTS vods_year ON vods (year)\n",
        "CREATE INDEX IF NOT EXISTS vods_season ON vods (season)\n",
        "CREATE INDEX IF NOT EXISTS vods_event_name ON vods (event_name)\n",
        "CREATE INDEX IF NOT EXISTS vods_seen ON vods (seen)\n",
        "CREATE INDEX IF NOT EXISTS vod_thumbnails_id ON vod_thumbnails (id)\n",
        "CREATE INDEX IF NOT EXISTS vod_thumbnails_url ON vod_thumbnails (url)\n",
        "CREATE INDEX IF NOT EXISTS vod_files_id ON vod_files (id)\n",
        "CREATE INDEX IF NOT EXISTS vod_url_share_id ON vod_url_share (id)\n",
        "CREATE INDEX IF NOT EXISTS vod_url_share_video_id ON vod_url_share (video_id)\n",
        "CREATE INDEX IF NOT EXISTS vod_url_share_type ON vod_url_share (type)\n"
            //"CREATE INDEX IF NOT EXISTS vod_file_ref_vod_id ON vod_file_ref (vod_id)\n",
            //"CREATE INDEX IF NOT EXISTS vod_file_ref_vod_file_id ON vod_file_ref (vod_file_id)\n"
    };

    static char const* const DropSql[] = {
        "DROP INDEX IF EXISTS vods_id",
        "DROP INDEX IF EXISTS vods_game",
        "DROP INDEX IF EXISTS vods_year",
        "DROP INDEX IF EXISTS vods_season",
        "DROP INDEX IF EXISTS vods_event_name",
        "DROP INDEX IF EXISTS vods_seen",
        "DROP INDEX IF EXISTS vod_thumbnails_id",
        "DROP INDEX IF EXISTS vod_thumbnails_url",
        "DROP INDEX IF EXISTS vod_url_share_id",
        "DROP INDEX IF EXISTS vod_url_share_video_id",
        "DROP INDEX IF EXISTS vod_url_share_type",
//        "DROP INDEX IF EXISTS vod_file_ref_vod_id",
//        "DROP INDEX IF EXISTS vod_file_ref_vod_file_id",


        "DROP TABLE IF EXISTS vod_file_ref",
        "DROP TABLE IF EXISTS vods",
        "DROP TABLE IF EXISTS vod_files",
        "DROP TABLE IF EXISTS vod_thumbnails ",
        "DROP TABLE IF EXISTS vod_url_share",
        "DROP TABLE IF EXISTS settings",
        "DROP TABLE IF EXISTS icons",
    };

    const int Version = 1;


    if (!q.exec("PRAGMA foreign_keys = ON")) {
        qCritical() << "failed to enable foreign keys" << q.lastError();
        setError(Error_CouldntCreateSqlTables);
        setStatus(Status_Error);
        return;
    }

    // https://codificar.com.br/blog/sqlite-optimization-faq/
    if (!q.exec("PRAGMA count_changes = OFF")) {
        qCritical() << "failed to disable change counting" << q.lastError();
        setError(Error_CouldntCreateSqlTables);
        setStatus(Status_Error);
        return;
    }

    if (!q.exec("PRAGMA temp_store = MEMORY")) {
        qCritical() << "failed to set temp store to memory" << q.lastError();
        setError(Error_CouldntCreateSqlTables);
        setStatus(Status_Error);
        return;
    }

    if (!q.exec("PRAGMA user_version") || !q.next()) {
        qCritical() << "failed to query user version" << q.lastError();
        setError(Error_CouldntCreateSqlTables);
        setStatus(Status_Error);
        return;
    }

    bool hasVersion = false;
    auto version = q.value(0).toInt(&hasVersion);
    if (hasVersion && version != Version) {
        for (size_t i = 0; i < _countof(DropSql); ++i) {
            if (!q.prepare(DropSql[i]) || !q.exec()) {
                qCritical() << "failed to drop table" << q.lastError();
                setError(Error_CouldntCreateSqlTables);
                setStatus(Status_Error);
            }
        }

        if (!q.exec(QStringLiteral("PRAGMA user_version = %1").arg(QString::number(Version)))) {
            qCritical() << "failed to set user version" << q.lastError();
            setError(Error_CouldntCreateSqlTables);
            setStatus(Status_Error);
            return;
        }
    }

    for (size_t i = 0; i < _countof(CreateSql); ++i) {
//        qDebug() << CreateSql[i];
        if (!q.prepare(CreateSql[i]) || !q.exec()) {
            qCritical() << "failed to create table" << q.lastError();
            setError(Error_CouldntCreateSqlTables);
            setStatus(Status_Error);
            return;
        }
    }

    const QString defaultValuesKeys[] = {
        QStringLiteral("download_marker")
    };

    const QString defaultValuesValues[] = {
        QDate().toString(Qt::ISODate)
    };

    // default values
    for (size_t i = 0; i < _countof(defaultValuesKeys); ++i) {
//        qDebug() << CreateSql[i];
        if (!q.prepare("INSERT INTO settings (key, value) VALUES (?, ?)")) {
            qCritical() << "failed to prepare settings insert" << q.lastError();
            setError(Error_SqlTableManipError);
            setStatus(Status_Error);
            return;
        }

        q.addBindValue(defaultValuesKeys[i]);
        q.addBindValue(defaultValuesValues[i]);

        if (!q.exec()) {
            qCritical() << "failed to insert into settings table" << q.lastError();
            setError(Error_SqlTableManipError);
            setStatus(Status_Error);
            return;
        }
    }

}

void
ScVodDataManager::setStatus(Status status) {
    if (m_Status != status) {
        m_Status = status;
        emit statusChanged();
    }
}

void
ScVodDataManager::setError(Error error) {
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
//            switch (game) {
//            case ScEnums::Game_Broodwar:
//                return QStringLiteral("Brood War");
//            case ScEnums::Game_Sc2:
//                return QStringLiteral("StarCraft II");
//            case ScEnums::Game_Unknown:
//                return tr("Misc");
//            default:
//                return tr("unknown game");
//            }
            switch (game) {
            case ScRecord::GameBroodWar:
                return QStringLiteral("Brood War");
            case ScRecord::GameSc2:
                return QStringLiteral("StarCraft II");
            case ScRecord::GameOverwatch:
                return tr("Overwatch");
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

            if (!q.prepare(QStringLiteral(
                               "SELECT file_name FROM icons WHERE url=?"))) {
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


void
ScVodDataManager::clear() {
    QMutexLocker g(&m_Lock);

    RETURN_IF_ERROR;

    // delete files
    QDir(m_MetaDataDir).removeRecursively();
    QDir(m_ThumbnailDir).removeRecursively();
    QDir(m_VodDir).removeRecursively();
    QDir().mkpath(m_MetaDataDir);
    QDir().mkpath(m_ThumbnailDir);
    QDir().mkpath(m_VodDir);
    QDir().mkpath(m_IconDir);

    // remove database entries
    QSqlQuery q(m_Database);
    if (!q.exec("BEGIN TRANSACTION")) {
        setError(Error_CouldntStartTransaction);
        setStatus(Status_Error);
        return;
    }

    if (!q.exec("DELETE FROM vods")) {
        setError(Error_SqlTableManipError);
        setStatus(Status_Error);
        return;
    }

    if (!q.exec("DELETE FROM vod_thumbnails")) {
        setError(Error_SqlTableManipError);
        setStatus(Status_Error);
        return;
    }

    if (!q.exec("DELETE FROM vod_files")) {
        setError(Error_SqlTableManipError);
        setStatus(Status_Error);
        return;
    }

    if (!q.exec("DELETE FROM vod_url_share")) {
        setError(Error_SqlTableManipError);
        setStatus(Status_Error);
        return;
    }

    if (!q.exec("DELETE FROM vod_file_ref")) {
        setError(Error_SqlTableManipError);
        setStatus(Status_Error);
        return;
    }

    if (!q.exec("DELETE FROM icons")) {
        setError(Error_SqlTableManipError);
        setStatus(Status_Error);
        return;
    }

    if (!q.exec("COMMIT TRANSACTION")) {
        setError(Error_CouldntEndTransaction);
        setStatus(Status_Error);
        return;
    }

    setDownloadMarker(QDate());

    tryRaiseVodsChanged();
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
        bool* exists) const {

    Q_ASSERT(exists);

    QSqlQuery q(m_Database);

    if (!q.prepare(QStringLiteral(
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
                       "    AND side1=?\n"
                       "    AND (side2 IS NULL OR side2=?)\n"
                       "    AND stage_name=?\n"))) {
        qCritical() << "failed to prepare match select" << q.lastError();
        return false;
    }

    q.addBindValue(event.name());
    q.addBindValue(event.year());
    q.addBindValue(event.game());
    q.addBindValue(event.season());
    q.addBindValue(QString::number(match.number()));
    q.addBindValue(match.date());
    q.addBindValue(match.side1());
    q.addBindValue(match.side2());
    q.addBindValue(stage.name());

    if (!q.exec()) {
        qCritical() << "failed to exec match select" << q.lastError();
        return false;
    }

    *exists = q.next();

    return true;
}


bool
ScVodDataManager::exists(const ScRecord& record, bool* exists) const {

    Q_ASSERT(exists);
    Q_ASSERT(record.isValid(
                 ScRecord::ValidEventName |
                 ScRecord::ValidStage |
                 ScRecord::ValidYear |
//                 ScRecord::ValidGame |
//                 ScRecord::ValidSeason |
                 ScRecord::ValidMatchName |
                 ScRecord::ValidMatchDate));

    QSqlQuery q(m_Database);

    if (!q.prepare(QStringLiteral(
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
                       "    AND stage_name=?\n"))) {
        qCritical() << "failed to prepare match select" << q.lastError();
        return false;
    }

    q.addBindValue(record.eventName);
    q.addBindValue(record.year);
    q.addBindValue(record.isValid(ScRecord::ValidGame) ? record.game : -1);
    q.addBindValue(record.isValid(ScRecord::ValidSeason) ? record.season : 0);
    q.addBindValue(record.matchName);
    q.addBindValue(record.matchDate);
    q.addBindValue(record.stage);

    if (!q.exec()) {
        qCritical() << "failed to exec match select" << q.lastError();
        return false;
    }

    *exists = q.next();

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
        exists(record, _exists);
    } else {
        *_exists = false;
    }
}

void
ScVodDataManager::addVods(const QList<ScRecord>& records) {
    QMutexLocker g(&m_Lock);
    if (_addVods(records)) {
        tryRaiseVodsChanged();
    }
}

bool
ScVodDataManager::_addVods(const QList<ScEvent>& events) {
    QSqlQuery q(m_Database);

    if (!q.exec("BEGIN TRANSACTION")) {
        qCritical() << "failed to start transaction" << q.lastError();
        return false;
    }

    QString cannonicalUrl, videoId;
    bool addedVods = false;
    for (const auto& event : events) {
        for (const auto& stage : event.data().stages) {
            for (const auto& match : stage.data().matches) {


                bool entryExists;
                if (!exists(event, stage, match, &entryExists) || entryExists) {
                    continue;
                }

                int urlType, startOffset;

                parseUrl(match.url(), &cannonicalUrl, &videoId, &urlType, &startOffset);
                qint64 urlShareId = 0;

                if (UT_Unknown != urlType) {
                    if (!q.prepare(QStringLiteral("SELECT id FROM vod_url_share WHERE video_id=? AND type=?"))) {
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
                        if (!q.prepare(QStringLiteral("INSERT INTO vod_url_share (video_id, url, type) VALUES (?, ?, ?)"))) {
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
                    if (!q.prepare(QStringLiteral("INSERT INTO vod_url_share (url, type) VALUES (?, ?)"))) {
                        qCritical() << "failed to prepare query" << q.lastError();
                        break;
                    }

                    q.addBindValue(match.url());
                    q.addBindValue(urlType);

                    if (!q.exec()) {
                        qCritical() << "failed to exec" << q.lastError();
                        continue;
                    }

                    urlShareId = qvariant_cast<qint64>(q.lastInsertId());
                }

                if (!q.prepare(
            QStringLiteral(
                                "INSERT INTO vods (\n"
                                "   event_name,\n"
                                "   event_full_title,\n"
                                "   season,\n"
                                "   game,\n"
                                "   year,\n"
                                "   stage_name,\n"
                                "   match_name,\n"
                                "   match_date,\n"
                                "   side1,\n"
                                "   side2,\n"
                                "   match_number,\n"
                                "   vod_url_share_id,\n"
                                "   offset\n"
                                ") VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)\n"))) {
                    qCritical() << "failed to prepare vod insert" << q.lastError();
                    continue;
                }

                q.addBindValue(event.name());
                //q.addBindValue(event.fullName());
                q.addBindValue(event.name());
                q.addBindValue(event.season());
                q.addBindValue(event.game());
                q.addBindValue(event.year());
                q.addBindValue(stage.name());
                q.addBindValue(stage.number());
                q.addBindValue(match.date());
                q.addBindValue(match.side1());
                q.addBindValue(match.side2());
                q.addBindValue(match.number());
                q.addBindValue(urlShareId);
                q.addBindValue(startOffset);

                if (!q.exec()) {
                    qCritical() << "failed to exec vod insert" << q.lastError();
                    qDebug() << "failed"
                             << match.date()
                             << match.side1()
                             << match.side2()
                             << urlShareId
                             << event.name()
                             << event.season()
                             << stage.name()
                             << stage.number()
                             << match.number()
                             << event.game()
                             << event.year()
                             << startOffset
                                ;

                    continue;
                }


                addedVods = true;
                qDebug() << "add"
                         << match.date()
                         << match.side1()
                         << match.side2()
                         << urlShareId
                         << event.name()
                         << event.season()
                         << stage.name()
                         << stage.number()
                         << match.number()
                         << event.game()
                         << event.year()
                         << startOffset
                            ;

            }
        }
    }

    if (!q.exec("END TRANSACTION")) {
        qCritical() << "failed to end transaction" << q.lastError();
        return false;
    }

    return addedVods;
}

bool
ScVodDataManager::_addVods(const QList<ScRecord>& records) {
    QSqlQuery q(m_Database);

    if (!q.exec("BEGIN TRANSACTION")) {
        qCritical() << "failed to start transaction" << q.lastError();
        return false;
    }

    QList<qint64> unknownGameRowids;
    QList<int> unknownGameIndices;

    QString cannonicalUrl, videoId;
    bool addedVods = false;
    for (int i = 0; i < records.size(); ++i) {
        const ScRecord& record = records[i];

        bool entryExists;
        if (!exists(record, &entryExists) || entryExists) {
            continue;
        }

        int urlType, startOffset;

        parseUrl(record.url, &cannonicalUrl, &videoId, &urlType, &startOffset);
        qint64 urlShareId = 0;

        if (UT_Unknown != urlType) {
            if (!q.prepare(QStringLiteral("SELECT id FROM vod_url_share WHERE video_id=? AND type=?"))) {
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
                if (!q.prepare(QStringLiteral("INSERT INTO vod_url_share (video_id, url, type) VALUES (?, ?, ?)"))) {
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
            if (!q.prepare(QStringLiteral("INSERT INTO vod_url_share (url, type) VALUES (?, ?)"))) {
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

        if (!q.prepare(
                    QStringLiteral(
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
                        "   offset\n"
                        ") VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)\n"))) {
            qCritical() << "failed to prepare vod insert" << q.lastError();
            continue;
        }

        q.addBindValue(record.eventName);
        q.addBindValue(record.eventFullName);
        q.addBindValue(record.isValid(ScRecord::ValidSeason) ? record.season : 0);
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

        if (!q.exec()) {
            qCritical() << "failed to exec vod insert" << q.lastError();
            continue;
        }

        if (!record.isValid(ScRecord::ValidGame)) {
            unknownGameRowids << qvariant_cast<qint64>(q.lastInsertId());
            unknownGameIndices << i;
        }

        addedVods = true;
    }

    // try to fix game column
    for (int i = 0; i < unknownGameIndices.size(); ++i) {
        const auto index = unknownGameIndices[i];
        const auto rowId = unknownGameRowids[i];
        const ScRecord& record = records[index];

        // see if any record with similar values has a valid game
        if (!q.prepare(QStringLiteral(
                           "SELECT\n"
                           "    game\n"
                           "FROM\n"
                           "    vods\n"
                           "WHERE\n"
                           "    event_name=?\n"
                           "    AND year=?\n"
                           "    AND season=?\n"
                           "    AND game!=-1\n"))) {
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

            if (!q.prepare(QStringLiteral(
                               "UPDATE vods SET game=? WHERE id=?"))) {
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

    if (!q.exec("END TRANSACTION")) {
        qCritical() << "failed to end transaction" << q.lastError();
        return false;
    }

    return addedVods;
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

                    // update title from vod
                    {
                        if (!q.prepare(QStringLiteral(
                    "UPDATE vod_url_share SET title=? WHERE id=?"))) {
                            qCritical() << "failed to prepare query" << q.lastError();
                            return;
                        }

                        q.addBindValue(vod.description().title());
                        q.addBindValue(r.vod_url_share_id);

                        if (!q.exec()) {
                            qCritical() << "failed to exec query" << q.lastError();
                            return;
                        }
                    }

                    // emit metaDataAvailable
                    {
                        if (!q.prepare(QStringLiteral(
                    "SELECT id FROM vods WHERE vod_url_share_id=?"))) {
                            qCritical() << "failed to prepare query" << q.lastError();
                            return;
                        }

                        q.addBindValue(r.vod_url_share_id);

                        if (!q.exec()) {
                            qCritical() << "failed to exec query" << q.lastError();
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

        QSqlQuery q(m_Database);
        if (download.progress() >= 1 && download.error() == VMVodEnums::VM_ErrorNone) {
            updateVodDownloadStatus(r.vod_file_id, download);
        }
    }
}

void ScVodDataManager::onFileDownloadChanged(qint64 token, const VMVodFileDownload& download) {
    QMutexLocker g(&m_Lock);
    auto it = m_VodmanFileRequests.find(token);
    if (it != m_VodmanFileRequests.end()) {
        const VodmanFileRequest& r = it.value();

        updateVodDownloadStatus(r.vod_file_id, download);
    }
}

void ScVodDataManager::updateVodDownloadStatus(
        qint64 vodFileId,
        const VMVodFileDownload& download) {
    QSqlQuery q(m_Database);

    if (!q.prepare(
        QStringLiteral("UPDATE vod_files SET progress=? WHERE id=?"))) {
        qCritical() << "failed to prepare query" << q.lastError();
        return;
    }

    q.addBindValue(download.progress());
    q.addBindValue(vodFileId);

    if (!q.exec()) {
        qCritical() << "failed to exec query" << q.lastError();
        return;
    }

    if (!q.prepare(
                QStringLiteral(
                    "SELECT\n"
                    "   v.id\n"
                    "FROM vod_file_ref r\n"
                    "INNER JOIN vods v ON v.id=r.vod_id\n"
                    "INNER JOIN vod_files f ON f.id=r.vod_file_id\n"
                    "WHERE f.id=?\n"))) {
        qCritical() << "failed to prepare query" << q.lastError();
        return;
    }

    q.addBindValue(vodFileId);

    if (!q.exec()) {
        qCritical() << "failed to exec query" << q.lastError();
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
    qDebug() << token << serviceErrorCode;
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

    QMutexLocker g(&m_Lock);

    QSqlQuery q(m_Database);

    if (!q.prepare(QStringLiteral(
"SELECT u.url, u.id FROM vods AS v INNER JOIN vod_url_share "
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
                        fetchMetaData(urlShareId, vodUrl);
                    }
                }
            } else {
                file.remove();
                if (download) {
                    fetchMetaData(urlShareId, vodUrl);
                }
            }
        } else {
            // vod links probably expired
            file.remove();
            if (download) {
                fetchMetaData(urlShareId, vodUrl);
            }
        }
    } else {
        if (download) {
            fetchMetaData(urlShareId, vodUrl);
        }
    }
}

void ScVodDataManager::fetchThumbnail(qint64 rowid) {
    RETURN_IF_ERROR;

    QMutexLocker g(&m_Lock);

    QSqlQuery q(m_Database);

    if (!q.prepare(QStringLiteral(
"SELECT v.thumbnail_id, u.url, u.id FROM vods AS v INNER JOIN vod_url_share "
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

    auto thumbnailId = q.value(0).isNull() ? 0 : qvariant_cast<qint64>(q.value(0));
    const auto vodUrl = q.value(1).toString();
    const auto vodUrlShareId = qvariant_cast<qint64>(q.value(2));
    if (thumbnailId) {
        // entry in tabe
        if (!q.prepare(
                    QStringLiteral("SELECT file_name, url FROM vod_thumbnails WHERE id=?"))) {
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
            auto thumbnailUrl = q.value(1).toString();
            fetchThumbnailFromUrl(thumbnailId, thumbnailUrl);
        }
    } else { // thumb nail id not set
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

                    if (!q.prepare(
                                QStringLiteral("SELECT id FROM vod_thumbnails WHERE url=?"))) {
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

                        if (!q.prepare(
                                    QStringLiteral("UPDATE vods SET thumbnail_id=? WHERE id=?"))) {
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
                            if (!q.prepare(
                                        QStringLiteral("INSERT INTO vod_thumbnails (url, file_name) VALUES (?, ?)"))) {
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
                            if (!q.prepare(
                                        QStringLiteral("UPDATE vods SET thumbnail_id=? WHERE id=?"))) {
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
                    fetchMetaData(vodUrlShareId, vodUrl);
                }
            } else {
                file.remove();
                fetchMetaData(vodUrlShareId, vodUrl);
            }
        } else {
            fetchMetaData(vodUrlShareId, vodUrl);
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
    r.token = m_Vodman->newToken();
    r.vod_url_share_id = urlShareId;
    m_VodmanMetaDataRequests.insert(r.token, r);
    m_Vodman->startFetchMetaData(r.token, url);
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

void ScVodDataManager::fetchVod(qint64 rowid, int formatIndex) {
    RETURN_IF_ERROR;

    if (formatIndex < 0) {
        qWarning() << "invalid format index, not fetching vod" << formatIndex;
        return;
    }


    QMutexLocker g(&m_Lock);

    QSqlQuery q(m_Database);

    if (!q.prepare(QStringLiteral(
"SELECT u.url, u.id FROM vods AS v INNER JOIN vod_url_share "
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

    const auto vodUrl = q.value(0).toString();
    const auto urlShareId = qvariant_cast<qint64>(q.value(1));

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
                if (formatIndex >= vod._formats().size()) {
                    qWarning() << "invalid format index (>), not fetching vod" << formatIndex;
                } else {
                    auto format = vod._formats()[formatIndex];
                    if (!q.prepare(QStringLiteral(
                                       "SELECT\n"
                                       "    f.file_name,\n"
                                       "    f.id,\n"
                                       "    f.progress,\n"
                                       "    f.width,\n"
                                       "    f.height,\n"
                                       "    f.format\n"
                                       "FROM vod_file_ref r\n"
                                       "INNER JOIN vods v ON v.id=r.vod_id\n"
                                       "INNER JOIN vod_files f ON f.id=r.vod_file_id\n"
                                       "WHERE v.id=? AND f.format=?\n"
                                       ))) {
                        qCritical() << "failed to prepare query" << q.lastError();
                        return;
                    }


                    q.addBindValue(rowid);
                    q.addBindValue(format.id());

                    if (!q.exec()) {
                        qCritical() << "failed to exec query" << q.lastError();
                        return;
                    }

                    QString vodFilePath;
                    qint64 vodFileId = -1;
                    if (q.next()) {
                        vodFilePath = m_VodDir + q.value(0).toString();
                        vodFileId = qvariant_cast<qint64>(q.value(1));
                        QFileInfo fi(vodFilePath);
                        if (fi.exists()) {
                            auto progress = q.value(2).toFloat();
                            if (progress >= 1) {
                                auto width = q.value(3).toInt();
                                auto height = q.value(4).toInt();
                                auto format = q.value(5).toString();
                                emit vodAvailable(rowid, vodFilePath, progress, fi.size(), width, height, format);
                            } else {
                                goto FetchVod;
                            }
                        } else {
                            goto FetchVod;
                        }
                    } else {
FetchVod:
                        if (vodFilePath.isEmpty()) {
                            QTemporaryFile tempFile(m_ThumbnailDir + QStringLiteral("XXXXXX.") + format.fileExtension());
                            tempFile.setAutoRemove(false);
                            if (tempFile.open()) {
                                auto thumbNailFilePath = tempFile.fileName();
                                auto tempFileName = QFileInfo(thumbNailFilePath).fileName();
                                vodFilePath = m_VodDir + tempFileName;

                                if (!q.prepare(QStringLiteral(
                                                   "INSERT INTO vod_files (file_name, format, width, height, progress) "
                                                   "VALUES (?, ?, ?, ?, ?)"))) {
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

                                vodFileId = qvariant_cast<qint64>(q.lastInsertId());

                                if (!q.prepare(QStringLiteral(
                                                   "INSERT INTO vod_file_ref (vod_id, vod_file_id) VALUES (?, ?)"))) {
                                    qCritical() << "failed to prepare query" << q.lastError();
                                    return;
                                }

                                q.addBindValue(rowid);
                                q.addBindValue(vodFileId);

                                if (!q.exec()) {
                                    qCritical() << "failed to exec query" << q.lastError();
                                    return;
                                }
                            }
                        }

//                        // reset progress in case the file was deleted
//                        if (!q.prepare(
//                            QStringLiteral("UPDATE vod_files SET progress=0 WHERE id=?"))) {
//                            qCritical() << "failed to prepare query" << q.lastError();
//                            return;
//                        }

//                        q.addBindValue(vodFileId);

//                        if (!q.exec()) {
//                            qCritical() << "failed to exec query" << q.lastError();
//                            return;
//                        }

                        for (auto& value : m_VodmanFileRequests) {
                            if (value.vod_url_share_id == urlShareId && value.formatIndex == formatIndex) {
                                return; // already underway
                            }
                        }

                        VodmanFileRequest r;
                        r.token = m_Vodman->newToken();
                        r.vod_url_share_id = urlShareId;
                        r.vod_file_id = vodFileId;
                        m_VodmanFileRequests.insert(r.token, r);
                        m_Vodman->startFetchFile(r.token, vod, formatIndex, vodFilePath);
                    }
                }
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

    if (!q.prepare(QStringLiteral(
                       "SELECT\n"
                       "    f.id\n"
                       "FROM vod_file_ref r\n"
                       "INNER JOIN vods v ON v.id=r.vod_id\n"
                       "INNER JOIN vod_files f ON f.id=r.vod_file_id\n"
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
        auto fileId = qvariant_cast<qint64>(q.value(0));
        auto beg = m_VodmanFileRequests.begin();
        auto end = m_VodmanFileRequests.end();
        for (auto it = beg; it != end; ) {
            if (it.value().vod_file_id == fileId) {
                m_Vodman->cancel(it.key());
                m_VodmanFileRequests.erase(it++);
                qDebug() << "cancel vod file request for file id" << fileId;
            } else {
                ++it;
            }
        }
    }
}


void
ScVodDataManager::cancelFetchMetaData(qint64 rowid) {
    RETURN_IF_ERROR;

    QMutexLocker g(&m_Lock);

    qDebug() << "cancel meta data fetch for" << rowid;

    QSqlQuery q(m_Database);

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
    RETURN_IF_ERROR;


    QMutexLocker g(&m_Lock);

    QSqlQuery q(m_Database);

    if (!q.prepare(QStringLiteral(
                      "SELECT\n"
                      "    f.file_name\n"
                      "FROM vod_file_ref r\n"
                      "INNER JOIN vods v ON v.id=r.vod_id\n"
                      "INNER JOIN vod_files f ON f.id=r.vod_file_id\n"
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
        if (QFile::remove(m_VodDir + fileName)) {
            qDebug() << "removed" << m_VodDir + fileName;
        }
    }
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
    QSqlQuery q(m_Database);
    if (!q.exec("SELECT value FROM settings WHERE key='download_marker'")) {
        qCritical() << "failed to fetch download marker from settings table" << q.lastError();
        return QDate();
    }

    // error handling
    if (q.next()) {
        return QDate::fromString(q.value(0).toString(), Qt::ISODate);
    }

    return QDate();
}

void
ScVodDataManager::setDownloadMarker(QDate value) {
    QSqlQuery q(m_Database);
    if (!q.prepare("UPDATE settings SET value=? WHERE key='download_marker'")) {
        qCritical() << "failed to prepare download marker update" << q.lastError();
        return;
    }

    q.addBindValue(value.toString(Qt::ISODate));

    // error handling
    if (!q.exec()) {
        qCritical() << "failed to exec download marker update" << q.lastError();
        return;
    }
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
    if (--m_SuspendedVodsChangedEventCount == 0){
        emit vodsChanged();
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
                      "    f.file_name,\n"
                      "    f.progress,\n"
                      "    f.width,\n"
                      "    f.height,\n"
                      "    f.format\n"
                      "FROM vod_file_ref r\n"
                      "INNER JOIN vods v ON v.id=r.vod_id\n"
                      "INNER JOIN vod_files f ON f.id=r.vod_file_id\n"
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

    QFileInfo fi;
    while (q.next()) {
        auto filePath = m_VodDir + q.value(0).toString();
        auto progress = q.value(1).toFloat();
        auto width = q.value(2).toInt();
        auto height = q.value(3).toInt();
        auto format = q.value(4).toString();

        fi.setFile(filePath);
        if (fi.exists()) {
            emit vodAvailable(rowid, filePath, progress, fi.size(), width, height, format);
        }
    }
}

qreal
ScVodDataManager::seen(const QVariantMap& filters) const {
    RETURN_IF_ERROR;

    if (filters.isEmpty()) {
        qWarning() << "no filters";
        return -1;
    }

    QMutexLocker g(&m_Lock);

    QSqlQuery q(m_Database);

    QString query;
    auto beg = filters.cbegin();
    auto end = filters.cend();

    for (auto it = beg; it != end; ++it) {
        if (query.size() > 0) {
            query += QStringLiteral(" AND ");
        }

        query += it.key() + QStringLiteral("=?");
//        qDebug() << it.key() << it.value();
    }

    if (!q.prepare(QStringLiteral(
                      "SELECT\n"
                      "    c.total, s.seen\n"
                      "FROM\n"
                      "    (SELECT COUNT(id) AS total FROM vods WHERE ") + query
                      + QStringLiteral(
                       ") AS c,\n"
                       "    (SELECT COUNT(id) AS seen FROM vods WHERE ") + query
                      + QStringLiteral(" AND seen=1) AS s\n"))) {
        qCritical() << "failed to prepare query" << q.lastError();
        return -1;
    }

    for (auto it = beg; it != end; ++it) {
        q.addBindValue(it.value());
    }

    for (auto it = beg; it != end; ++it) {
        q.addBindValue(it.value());
    }

    if (!q.exec()) {
        qCritical() << "failed to exec query" << q.lastError();
        return -1;
    }

    q.next();

    auto total = q.value(0).toInt();
    auto seen = q.value(1).toInt();

    auto result = static_cast<qreal>(seen) / total;

//    qDebug() << result;
    return result;
}

void
ScVodDataManager::setSeen(const QVariantMap& filters, bool value) {
    RETURN_IF_ERROR;

    if (filters.isEmpty()) {
        qWarning() << "no filters";
        return;
    }

    QMutexLocker g(&m_Lock);

    QSqlQuery q(m_Database);

    QString query;
    auto beg = filters.cbegin();
    auto end = filters.cend();

    for (auto it = beg; it != end; ++it) {
        if (query.size() > 0) {
            query += QStringLiteral(" AND ");
        }

        query += it.key() + QStringLiteral("=?");
//        qDebug() << it.key() << it.value();
    }

    query = QStringLiteral("UPDATE vods SET seen=? WHERE\n") + query;


    if (!q.prepare(query)) {
        qCritical() << "failed to prepare query" << q.lastError();
        return;
    }

    q.addBindValue(value);

    for (auto it = beg; it != end; ++it) {
        q.addBindValue(it.value());
    }

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


