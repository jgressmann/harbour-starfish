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

#pragma once

#include <QObject>
#include <QDate>
#include <QSize>
#include <QSharedPointer>
#include <QVariant>
#include <functional>
#include "ScDatabaseStoreQueueTypes.h"

class ScVodDataManager;
class ScUrlShareItem;

class ScMatchItem : public QObject
{
    Q_OBJECT
    Q_PROPERTY(qint64 rowId READ rowId CONSTANT)
    Q_PROPERTY(ScUrlShareItem* urlShare READ urlShare CONSTANT)
    Q_PROPERTY(bool seen READ seen WRITE setSeen NOTIFY seenChanged)
//    Q_PROPERTY(bool deleted READ deleted WRITE setDeleted NOTIFY deletedChanged)
    Q_PROPERTY(QString eventFullName READ eventFullName CONSTANT)
    Q_PROPERTY(QString eventName READ eventName CONSTANT)
    Q_PROPERTY(QString stageName READ stageName CONSTANT)
    Q_PROPERTY(QString matchName READ matchName CONSTANT)
    Q_PROPERTY(QString side1 READ side1 CONSTANT)
    Q_PROPERTY(QString side2 READ side2 CONSTANT)
    Q_PROPERTY(int race1 READ race1 CONSTANT)
    Q_PROPERTY(int race2 READ race2 CONSTANT)
    Q_PROPERTY(QDate matchDate READ matchDate CONSTANT)
    Q_PROPERTY(int videoStartOffset READ videoStartOffset CONSTANT)
    Q_PROPERTY(int videoEndOffset READ videoEndOffset NOTIFY videoEndOffsetChanged)
    Q_PROPERTY(int videoPlaybackOffset READ videoPlaybackOffset NOTIFY videoPlaybackOffsetChanged)
    Q_PROPERTY(int season READ season CONSTANT)
    Q_PROPERTY(int year READ year CONSTANT)
    Q_PROPERTY(int matchNumber READ matchNumber CONSTANT)
    Q_PROPERTY(bool deleted READ deleted CONSTANT)

public:
    explicit ScMatchItem(qint64 rowid, ScVodDataManager *parent, QSharedPointer<ScUrlShareItem>&& urlShareItem);

public:
    qint64 rowId() const { return m_RowId; }
    ScUrlShareItem* urlShare() const { return m_UrlShareItem.data(); }
    bool seen() const { return m_seen; }
    bool deleted() const { return m_Deleted; }
    QString eventFullName() const { return m_EventFullName; }
    QString eventName() const { return m_EventName; }
    QString stageName() const { return m_stageName; }
    QString side1() const { return m_side1; }
    QString side2() const { return m_side2; }
    int race1() const { return m_race1; }
    int race2() const { return m_race2; }
    QDate matchDate() const { return m_matchDate; }
    int videoStartOffset() const { return m_VideoStartOffset; }
    int videoEndOffset() const { return m_VideoEndOffset; }
    int videoPlaybackOffset() const { return qMax(m_VideoStartOffset, qMin(m_VideoPlaybackOffset, m_VideoEndOffset)); }
    void setVideoPlaybackOffset(int value);
    int season() const { return m_season; }
    int year() const { return m_year; }
    QString matchName() const { return m_matchName; }
    int matchNumber() const { return m_match_number; }
    void onSeenAvailable(bool value);
//    void onDeletedAvailable(bool value);
    void onVodEndAvailable(int endOffsetS);

signals:
    void seenChanged();
//    void deletedChanged();
    void videoEndOffsetChanged();
    void videoPlaybackOffsetChanged();
    void startProcessDatabaseStoreQueue(int transactionId, QString sql, ScSqlParamList args);

private:
    struct Data
    {
        int playbackOffset;
        bool seen;
//        bool deleted;

        Data();
    };

    using DatabaseCallback = std::function<void(qint64 insertIdOrNumRows, bool error)>;

private slots:
    void reset();
    void databaseStoreCompleted(int token, qint64 insertIdOrNumRowsAffected, int error, QString errorDescription);
    void onLengthChanged();

private:
    ScVodDataManager* manager() const;
    bool fetch(Data* data) const;
    bool fetchSeen(Data* data) const;
    bool fetchVideoPlaybackOffset(Data* data) const;
    void setSeen(bool seen);
    void setSeenMember(bool value);
//    void setDeleted(bool seen);
//    void setDeletedMember(bool value);


private:
    QHash<int, DatabaseCallback> m_PendingDatabaseStores;
    QSharedPointer<ScUrlShareItem> m_UrlShareItem;
    QString m_EventFullName;
    QString m_EventName;
    QString m_stageName;
    QString m_matchName;
    QString m_side1;
    QString m_side2;
    qint64 m_RowId;
    QDate m_matchDate;
    int m_race1;
    int m_race2;
    int m_VideoStartOffset;
    int m_VideoEndOffset;
    int m_VideoPlaybackOffset;
    int m_season;
    int m_year;
    int m_match_number;
    int m_stage_rank;
    bool m_seen;
    bool m_Deleted;
};
