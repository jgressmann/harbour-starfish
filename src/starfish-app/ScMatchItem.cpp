#include "ScMatchItem.h"
#include "ScVodDataManager.h"
#include "ScRecord.h"
#include "ScDatabaseStoreQueue.h"

#include <QSqlQuery>
#include <QSqlError>

namespace
{
ScMatchItem::Race getRace(int r)
{
    switch (r) {
    case ScRecord::RaceProtoss:
        return ScMatchItem::Protoss;
    case ScRecord::RaceRandom:
        return ScMatchItem::Random;
    case ScRecord::RaceTerran:
        return ScMatchItem::Terran;
    case ScRecord::RaceZerg:
        return ScMatchItem::Zerg;
    default:
        return ScMatchItem::Unknown;
    }
}
} // anon

ScMatchItem::ScMatchItem(qint64 rowid, ScVodDataManager *parent)
    : QObject(parent)
    , m_RowId(rowid)
{
    m_FileSize = -1;
    m_race1 = -1;
    m_race2 = -1;
    m_VideoStartOffset = -1;
    m_VideoEndOffset = -1;
    m_season = -1;
    m_year = -1;
    m_match_number = -1;
    m_stage_rank = -1;
    m_Width = -1;
    m_Height = -1;
    m_Length = -1;
    m_Progress = -1;
    m_seen = false;

    connect(parent, &ScVodDataManager::vodsChanged, this, &ScMatchItem::reset);
    connect(parent, &ScVodDataManager::seenChanged, this, &ScMatchItem::reset);
    connect(parent, &ScVodDataManager::seenAvailable, this, &ScMatchItem::onSeenAvailable);
    connect(parent, &ScVodDataManager::titleAvailable, this, &ScMatchItem::onTitleAvailable);
    connect(parent, &ScVodDataManager::vodEndAvailable, this, &ScMatchItem::onVodEndAvailable);

    auto x = manager()->databaseStoreQueue();
    // database store queue->this
    connect(x, &ScDatabaseStoreQueue::completed, this, &ScMatchItem::databaseStoreCompleted);
    // this->database store queue
    connect(this, &ScMatchItem::startProcessDatabaseStoreQueue, x, &ScDatabaseStoreQueue::process);

    QSqlQuery q(manager()->database());
    auto sql = QStringLiteral("SELECT * from url_share_vods WHERE id=?");
    if (q.prepare(sql)) {
        q.addBindValue(rowid);
        if (q.exec()) {
            if (q.next()) {
                m_Url = q.value(1).toString();
                m_side1 = q.value(6).toString();
                m_side2 = q.value(7).toString();
                m_race1 = q.value(8).toInt();
                m_race2 = q.value(9).toInt();
                m_EventName = q.value(11).toString();
                m_EventFullName = q.value(12).toString();
                m_season = q.value(13).toInt();
                m_stageName = q.value(14).toString();
                m_matchName = q.value(15).toString();
                m_matchDate = q.value(16).toDate();
                // game
                m_year = q.value(18).toInt();
                m_VideoStartOffset = q.value(18).toInt();
                // length
                // seen
                m_match_number = q.value(20).toInt();
                // thumbnailid
                m_stage_rank = q.value(22).toInt();
            } else {
                qDebug() << "no data for rowid" << m_RowId;
            }
        } else {
            qDebug() << "failed to exec, error:" << q.lastError();
        }
    } else {
        qDebug() << "failed to prepare, error:" << q.lastError();
    }

    Data data;
    if (fetch(&data)) {
        m_seen = data.seen;
        m_Title = data.title;
        m_Length = data.length;
        manager()->fetchVodEnd(m_RowId, m_VideoStartOffset, m_Length);
    }
}

ScVodDataManager*
ScMatchItem::manager() const
{
    return qobject_cast<ScVodDataManager*>(parent());
}

void ScMatchItem::onSeenChanged()
{
    Data data;
    if (fetch(&data) && data.seen != m_seen) {
        m_seen = data.seen;
        emit seenChanged();
    }
}

bool ScMatchItem::fetch(Data* data) const
{
    Q_ASSERT(data);
    QSqlQuery q(manager()->database());
    auto sql = QStringLiteral("SELECT seen, title, length FROM url_share_vods WHERE id=?");
    if (q.prepare(sql)) {
        q.addBindValue(m_RowId);
        if (q.exec()) {
            if (q.next()) {
                data->seen = q.value(0).toInt() != 0;
                data->title = q.value(1).toString();
                data->length = q.value(2).toInt();
//                data->url = q.value(3).toString();
                return true;
            } else {
                qDebug() << "no data for rowid" << m_RowId;
            }
        } else {
            qDebug() << "failed to exec, error:" << q.lastError();
        }
    } else {
        qDebug() << "failed to prepare, error:" << q.lastError();
    }

    return false;
}


void ScMatchItem::setSeen(bool value)
{
    auto transactionId = manager()->databaseStoreQueue()->newTransactionId();
    m_PendingDatabaseStores.insert(transactionId, [=] (qint64 rows, bool error)
    {
        if (!error && rows) {
            emit seenChanged();
        }
    });
    auto sql = QStringLiteral("UPDATE vods SET seen=? WHERE id=?");
    emit startProcessDatabaseStoreQueue(transactionId, sql, {value, m_RowId});
    emit startProcessDatabaseStoreQueue(transactionId, {}, {});
}

void ScMatchItem::reset()
{
    Data data;
    if (fetch(&data)) {
        if (m_seen != data.seen) {
            m_seen = data.seen;
            emit seenChanged();
        }

        if (m_Title != data.title) {
            m_Title = data.title;
            emit titleChanged();
        }

        if (m_Length != data.length) {
            m_Length = data.length;
            emit lengthChanged();
            manager()->fetchVodEnd(m_RowId, m_VideoStartOffset, m_Length);
        }
    }
}

void ScMatchItem::onTitleAvailable(qint64 rowid, QString title)
{
    if (rowid == m_RowId) {
        if (m_Title != title) {
            m_Title = title;
            emit titleChanged();
        }
    }
}

void ScMatchItem::onSeenAvailable(qint64 rowid, qreal seen)
{
    if (rowid == m_RowId) {
        auto s = seen != 0;
        if (m_seen != s) {
            m_seen = s;
            emit seenChanged();
        }
    }
}

void ScMatchItem::onVodEndAvailable(qint64 rowid, int endOffsetS)
{
    if (rowid == m_RowId) {
        if (m_VideoEndOffset != endOffsetS) {
            m_VideoEndOffset = endOffsetS;
            emit videoEndOffsetChanged();
        }
    }
}


void
ScMatchItem::databaseStoreCompleted(int token, qint64 insertIdOrNumRowsAffected, int error, QString errorDescription)
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
