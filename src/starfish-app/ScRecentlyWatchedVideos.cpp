/* The MIT License (MIT)
 *
 * Copyright (c) 2019 Jean Gressmann <jean@0x42.de>
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


#include "ScRecentlyWatchedVideos.h"
#include "ScVodDataManager.h"
#include "ScDatabaseStoreQueue.h"


#include <QSqlQuery>
#include <QSqlError>
#include <QDebug>
#include <QFile>

namespace
{
const QString s_VodId = QStringLiteral("vod_id");
const QString s_Url = QStringLiteral("url");
}


const QHash<int, QByteArray> ScRecentlyWatchedVideos::ms_RoleNames = {
    { Qt::UserRole + 0, "video_id" },
    { Qt::UserRole + 1, "url" },
    { Qt::UserRole + 2, "thumbnail_path" },
    { Qt::UserRole + 3, "offset" },
//    { Qt::UserRole + 4, "seen" },
};

ScRecentlyWatchedVideos::ScRecentlyWatchedVideos(ScVodDataManager* parent)
    : QAbstractListModel(parent)
{
    m_Count = 10;
    m_IndexCache = -1;
    m_RowCount = -1;
    m_RowCache.resize(ms_RoleNames.size());

    // database store queue->this
    connect(parent, &ScVodDataManager::vodsChanged, this, &ScRecentlyWatchedVideos::reset);
    connect(parent, &ScVodDataManager::seenChanged, this, &ScRecentlyWatchedVideos::reset);

    auto x = manager()->databaseStoreQueue();
    // database store queue->this
    connect(x, &ScDatabaseStoreQueue::completed, this, &ScRecentlyWatchedVideos::databaseStoreCompleted);
    // this->database store queue
    connect(this, &ScRecentlyWatchedVideos::startProcessDatabaseStoreQueue, x, &ScDatabaseStoreQueue::process);
}

void
ScRecentlyWatchedVideos::setCount(int value) {
    if (value <= 0) {
        qWarning().nospace() << "count must be greater than 0" << value;
        return;
    }

    if (value != m_Count) {
        m_Count = value;
        emit countChanged();
        reset();
    }
}

int
ScRecentlyWatchedVideos::rowCount(const QModelIndex &/*parent*/) const
{
    if (m_RowCount == -1) {
        QSqlQuery q(manager()->database());
        if (q.prepare(QStringLiteral("SELECT MIN(?, COUNT(*)) FROM recently_watched WHERE seen=0"))) {
            q.addBindValue(m_Count);
            if (q.exec()) {
                if (q.next()) {
                    m_RowCount = q.value(0).toInt();
                } else {
                    qWarning().nospace().noquote() << "failed get row, error: " << q.lastError();
                    return 0;
                }
            } else {
                qWarning().nospace().noquote() << "failed exec, error: " << q.lastError();
                return 0;
            }
        } else {
            qWarning().nospace().noquote() << "failed prepare, error: " << q.lastError();
            return 0;
        }
    }

    return m_RowCount;
}


QVariant
ScRecentlyWatchedVideos::data(const QModelIndex &index, int role) const
{
    int column = Qt::DisplayRole == role ? index.column() : role - Qt::UserRole;
    auto row = index.row();
    if (row >= 0 && row < m_RowCount && column >= 0 && column < ms_RoleNames.size()) {
        if (m_IndexCache != row) {
            QSqlQuery q(manager()->database());
            auto sql = QStringLiteral("SELECT * from recently_watched WHERE seen=0 ORDER BY modified DESC LIMIT %2, 1").arg(QString::number(row));
            if (q.exec(sql)) {
                if (q.next()) {
                    m_IndexCache = row;
                    for (int i = 0; i < m_RowCache.size(); ++i) {
                        m_RowCache[i] = q.value(i);
                    }
                } else {
                    qWarning().nospace().noquote() << "failed get row " << row << ", error: " << q.lastError();
                    return QVariant();
                }
            } else {
                qWarning().nospace().noquote() << "failed select row, sql: " << sql << ", error: " << q.lastError();
                return QVariant();
            }
        }

        const auto& value = m_RowCache[column];
//            qDebug().nospace().noquote() << "row " << row << " column " << column << " " << value;
        return value;
    }

    return QVariant();
}

void
ScRecentlyWatchedVideos::add(const QVariant& k, bool seen)
{
    KeyType t;
    if (!keyType(k, &t)) {
        qWarning().nospace().noquote() << "invalid key";
        return;
    }

    auto transactionId = manager()->databaseStoreQueue()->newTransactionId();

    // save one model reset as setOffset currently always follows add (see QML)

//    m_PendingDatabaseStores.insert(transactionId, [=] (qint64 rows, bool error)
//    {
//        if (!error && rows) {
//            reset();
//        }
//    });

    auto query = QStringLiteral("INSERT INTO recently_watched (%1, modified, playback_offset, seen) VALUES (?, ?, ?, ?)").arg(Vod == t ? s_VodId : s_Url);
    emit startProcessDatabaseStoreQueue(transactionId, query, {k, QDateTime::currentDateTime(), 0, seen});
    emit startProcessDatabaseStoreQueue(transactionId, {}, {});
}

void
ScRecentlyWatchedVideos::remove(const QVariant& k)
{
    KeyType t;
    if (!keyType(k, &t)) {
        qWarning().nospace().noquote() << "invalid key";
        return;
    }

    deleteExistingThumbnailFile(k);

    auto transactionId = manager()->databaseStoreQueue()->newTransactionId();
    m_PendingDatabaseStores.insert(transactionId, [=] (qint64 rows, bool error)
    {
        if (!error && rows) {
            reset();
        }
    });

    auto query = QStringLiteral("DELETE FROM recently_watched WHERE %1=?").arg(Vod == t ? s_VodId : s_Url);
    emit startProcessDatabaseStoreQueue(transactionId, query, {k});

    emit startProcessDatabaseStoreQueue(transactionId, {}, {});
}

void
ScRecentlyWatchedVideos::setOffset(const QVariant& k, int offset)
{
    KeyType t;
    if (!keyType(k, &t)) {
        qWarning().nospace().noquote() << "invalid key";
        return;
    }

    auto transactionId = manager()->databaseStoreQueue()->newTransactionId();
    m_PendingDatabaseStores.insert(transactionId, [=] (qint64 rows, bool error)
    {
        if (!error && rows) {
            reset();
        }
    });

    auto query = QStringLiteral("UPDATE recently_watched SET playback_offset=?, modified=? WHERE %1=?").arg(Vod == t ? s_VodId : s_Url);
    emit startProcessDatabaseStoreQueue(transactionId, query, {offset, QDateTime::currentDateTime(), k});
    emit startProcessDatabaseStoreQueue(transactionId, {}, {});
}


void
ScRecentlyWatchedVideos::setThumbnailPath(const QVariant& k, const QString& thumbnailPath)
{
    KeyType t;
    if (!keyType(k, &t)) {
        qWarning().nospace().noquote() << "invalid key";
        return;
    }

    deleteExistingThumbnailFile(k);

    auto transactionId = manager()->databaseStoreQueue()->newTransactionId();
    m_PendingDatabaseStores.insert(transactionId, [=] (qint64 rows, bool error)
    {
        if (!error && rows) {
            reset();
        }
    });

    auto query = QStringLiteral("UPDATE recently_watched SET thumbnail_path=?, modified=? WHERE %1=?").arg(Vod == t ? s_VodId : s_Url);
    emit startProcessDatabaseStoreQueue(transactionId, query, {thumbnailPath, QDateTime::currentDateTime(), k});
    emit startProcessDatabaseStoreQueue(transactionId, {}, {});
}

void
ScRecentlyWatchedVideos::clear()
{
    QSqlQuery q(manager()->database());
    auto sql = QStringLiteral("SELECT thumbnail_path FROM recently_watched WHERE thumbnail_path IS NOT NULL");
    if (q.exec(sql)) {
        while (q.next()) {
            auto thumbnailPath = q.value(0).toString();
            if (QFile::remove(thumbnailPath)) {
                qInfo().nospace().noquote() << "removed " << thumbnailPath;
            }
        }
    } else {
        qWarning().nospace().noquote() << "failed exec sql: " << sql << ", error: " << q.lastError();
    }

    auto transactionId = manager()->databaseStoreQueue()->newTransactionId();
    m_PendingDatabaseStores.insert(transactionId, [=] (qint64 rows, bool error)
    {
        if (!error && rows) {
            reset();
        }
    });
    sql = QStringLiteral("DELETE FROM recently_watched");
    emit startProcessDatabaseStoreQueue(transactionId, sql, {});
    emit startProcessDatabaseStoreQueue(transactionId, {}, {});
}

void
ScRecentlyWatchedVideos::databaseStoreCompleted(int token, qint64 insertIdOrNumRowsAffected, int error, QString errorDescription)
{
    auto it = m_PendingDatabaseStores.find(token);
    if (it == m_PendingDatabaseStores.end()) {
        return;
    }

    auto callback = it.value();
    m_PendingDatabaseStores.erase(it);

    if (error) {
        qWarning() << "database insert/update/delete failed code" << error << "desc" << errorDescription;
        callback(insertIdOrNumRowsAffected, true);
    } else {
        callback(insertIdOrNumRowsAffected, false);
    }
}

void
ScRecentlyWatchedVideos::reset()
{
    m_RowCount = -1;
    m_IndexCache = -1;
    beginResetModel();
    endResetModel();
}

ScVodDataManager*
ScRecentlyWatchedVideos::manager() const
{
    return qobject_cast<ScVodDataManager*>(parent());
}

QHash<int,QByteArray>
ScRecentlyWatchedVideos::roleNames() const
{
    return ms_RoleNames;
}

void
ScRecentlyWatchedVideos::deleteExistingThumbnailFile(const QVariant& k) const
{
    KeyType t;
    auto r = keyType(k, &t);
    Q_ASSERT(r);
    (void)r;

    QSqlQuery q(manager()->database());
    auto sql = QStringLiteral("SELECT thumbnail_path FROM recently_watched WHERE %1=?").arg(Vod == t ? s_VodId : s_Url);
    if (q.prepare(sql)) {
        q.addBindValue(k);
        if (q.exec()) {
            if (q.next()) {
                auto thumbnailPath = q.value(0).toString();
                if (QFile::remove(thumbnailPath)) {
                    qInfo().nospace().noquote() << "removed " << thumbnailPath;
                }
            }
        } else {
            qWarning().nospace().noquote() << "failed exec sql: " << sql << ", error: " << q.lastError();
        }
    } else {
        qWarning().nospace().noquote() << "failed prepare sql: " << sql << ", error: " << q.lastError();
    }
}

int
ScRecentlyWatchedVideos::offset(const QVariant& k) const
{
    KeyType t;
    if (!keyType(k, &t)) {
        qWarning().nospace().noquote() << "invalid key";
        return -1;
    }

    QSqlQuery q(manager()->database());
    auto sql = QStringLiteral("SELECT playback_offset FROM recently_watched WHERE %1=?").arg(Vod == t ? s_VodId : s_Url);
    if (q.prepare(sql)) {
        q.addBindValue(k);
        if (q.exec()) {
            if (q.next()) {
                return q.value(0).toInt();
            }
        } else {
            qWarning().nospace().noquote() << "failed exec sql: " << sql << ", error: " << q.lastError();
        }
    } else {
        qWarning().nospace().noquote() << "failed prepare sql: " << sql << ", error: " << q.lastError();
    }

    return -1;
}

QString
ScRecentlyWatchedVideos::getThumbnailPath(const QVariant& k) const
{
    KeyType t;
    if (!keyType(k, &t)) {
        qWarning().nospace().noquote() << "invalid key";
        return QString();
    }

    QSqlQuery q(manager()->database());
    auto sql = QStringLiteral("SELECT thumbnail_path FROM recently_watched WHERE %1=?").arg(Vod == t ? s_VodId : s_Url);
    if (q.prepare(sql)) {
        q.addBindValue(k);
        if (q.exec()) {
            if (q.next()) {
                return q.value(0).toString();
            }
        } else {
            qWarning().nospace().noquote() << "failed exec sql: " << sql << ", error: " << q.lastError();
        }
    } else {
        qWarning().nospace().noquote() << "failed prepare sql: " << sql << ", error: " << q.lastError();
    }

    return QString();
}

bool
ScRecentlyWatchedVideos::keyType(const QVariant& v, KeyType* keytype)
{
    Q_ASSERT(keytype);
    switch (v.type()) {
    case QVariant::String:
        *keytype = Url;
        return true;
    case QVariant::Invalid:
        return false;
    default:
        *keytype = Vod;
        return true;
    }
}
