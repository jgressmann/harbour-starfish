#include "ScMatchItem.h"
#include "ScVodDataManager.h"
#include "ScRecord.h"
#include "ScDatabaseStoreQueue.h"
#include "ScApp.h"

#include <QSqlQuery>
#include <QSqlError>

ScMatchItem::Data::Data()
{
    seen = false;
}

ScMatchItem::ScMatchItem(qint64 rowid, ScVodDataManager *parent, QSharedPointer<ScUrlShareItem>&& urlShareItem)
    : QObject(parent)
    , m_UrlShareItem(std::move(urlShareItem))
    , m_RowId(rowid)
{
    m_race1 = -1;
    m_race2 = -1;
    m_VideoStartOffset = -1;
    m_VideoEndOffset = -1;
    m_VideoPlaybackOffset = -1;
    m_season = -1;
    m_year = -1;
    m_match_number = -1;
    m_stage_rank = -1;
    m_seen = false;

    connect(parent, &ScVodDataManager::vodsChanged, this, &ScMatchItem::reset);
    connect(parent, &ScVodDataManager::seenChanged, this, &ScMatchItem::reset);
    connect(m_UrlShareItem.data(), &ScUrlShareItem::vodLengthChanged, this, &ScMatchItem::onLengthChanged);

    auto x = manager()->databaseStoreQueue();
    // database store queue->this
    connect(x, &ScDatabaseStoreQueue::completed, this, &ScMatchItem::databaseStoreCompleted);
    // this->database store queue
    connect(this, &ScMatchItem::startProcessDatabaseStoreQueue, x, &ScDatabaseStoreQueue::process);

    QSqlQuery q(manager()->database());
    auto sql = QStringLiteral(
"SELECT\n"
"   side1_name,\n"
"   side2_name,\n"
"   side1_race,\n"
"   side2_race,\n"
"   event_name,\n"
"   event_full_name,\n"
"   season,\n"
"   stage_name,\n"
"   match_name,\n"
"   match_date,\n"
"   year,\n"
"   offset,\n"
"   match_number,\n"
"   stage_rank\n"
"FROM vods WHERE id=?");
    if (q.prepare(sql)) {
        q.addBindValue(rowid);
        if (q.exec()) {
            if (q.next()) {
                m_side1 = q.value(0).toString();
                m_side2 = q.value(1).toString();
                m_race1 = q.value(2).toInt();
                m_race2 = q.value(3).toInt();
                m_EventName = q.value(4).toString();
                m_EventFullName = q.value(5).toString();
                m_season = q.value(6).toInt();
                m_stageName = q.value(7).toString();
                m_matchName = q.value(8).toString();
                m_matchDate = q.value(9).toDate();
                m_year = q.value(10).toInt();
                m_VideoStartOffset = q.value(11).toInt();
                m_match_number = q.value(12).toInt();
                m_stage_rank = q.value(13).toInt();
            } else {
                qDebug() << "no data for rowid" << m_RowId;
            }
        } else {
            qDebug() << "failed to exec, error:" << q.lastError();
        }
    } else {
        qDebug() << "failed to prepare, error:" << q.lastError();
    }

    reset();
}

ScVodDataManager*
ScMatchItem::manager() const
{
    return qobject_cast<ScVodDataManager*>(parent());
}

bool ScMatchItem::fetch(Data* data) const
{
    return fetchSeen(data) && fetchVideoPlaybackOffset(data);
}

bool ScMatchItem::fetchSeen(Data* data) const
{
    Q_ASSERT(data);
    QSqlQuery q(manager()->database());
    auto sql = QStringLiteral("SELECT seen FROM vods WHERE id=?");
    if (q.prepare(sql)) {
        q.addBindValue(m_RowId);
        if (q.exec()) {
            if (q.next()) {
                data->seen = q.value(0).toInt() != 0;
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

bool ScMatchItem::fetchVideoPlaybackOffset(Data* data) const
{
    Q_ASSERT(data);
    QSqlQuery q(manager()->database());
    auto sql = QStringLiteral("SELECT playback_offset FROM recently_watched WHERE vod_id=?");
    if (q.prepare(sql)) {
        q.addBindValue(m_RowId);
        if (q.exec()) {
            if (q.next()) {
                data->playbackOffset = q.value(0).toInt();
            } else {
                data->playbackOffset = -1;
            }
            return true;
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
            setSeenMember(value);
        }
    });
    auto sql = QStringLiteral("UPDATE vods SET seen=? WHERE id=?");
    emit startProcessDatabaseStoreQueue(transactionId, sql, {value, m_RowId});
    emit startProcessDatabaseStoreQueue(transactionId, {}, {});
}

void ScMatchItem::setSeenMember(bool value)
{
    if (value != m_seen) {
        m_seen = value;
        emit seenChanged();
    }
}

void ScMatchItem::onSeenAvailable(bool seen)
{
    setSeenMember(seen);
}

void ScMatchItem::onVodEndAvailable(int endOffsetS)
{
    if (m_VideoEndOffset != endOffsetS) {
        m_VideoEndOffset = endOffsetS;
        emit videoEndOffsetChanged();
        emit videoPlaybackOffsetChanged();
    }
}

void ScMatchItem::setVideoPlaybackOffset(int value)
{
    if (value != m_VideoPlaybackOffset) {
        m_VideoPlaybackOffset = value;
        emit videoPlaybackOffsetChanged();
    }
}

void ScMatchItem::reset()
{
    Data data;
    if (fetch(&data)) {
        setSeenMember(data.seen);
        setVideoPlaybackOffset(data.playbackOffset);
    }

    onLengthChanged();
}

void ScMatchItem::onLengthChanged()
{
    manager()->fetchVodEnd(m_RowId, m_VideoStartOffset, m_UrlShareItem->vodLength());
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


