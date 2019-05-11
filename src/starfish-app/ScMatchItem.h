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
    Q_PROPERTY(int season READ season CONSTANT)
    Q_PROPERTY(int year READ year CONSTANT)
    Q_PROPERTY(int matchNumber READ matchNumber CONSTANT)

public:
    explicit ScMatchItem(qint64 rowid, ScVodDataManager *parent, QSharedPointer<ScUrlShareItem>&& urlShareItem);

public:
    qint64 rowId() const { return m_RowId; }
    ScUrlShareItem* urlShare() const { return m_UrlShareItem.data(); }
    bool seen() const { return m_seen; }
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
    int season() const { return m_season; }
    int year() const { return m_year; }
    QString matchName() const { return m_matchName; }
    int matchNumber() const { return m_match_number; }
    void onSeenAvailable(bool seen);
    void onVodEndAvailable(int endOffsetS);

signals:
    void seenChanged();
    void videoEndOffsetChanged();
    void startProcessDatabaseStoreQueue(int transactionId, QString sql, ScSqlParamList args);

private:
    struct Data
    {
        bool seen;

        Data();
    };

    using DatabaseCallback = std::function<void(qint64 insertIdOrNumRows, bool error)>;

private slots:
    void reset();
//    void onSeenAvailable(qint64 rowid, qreal seen);
    void databaseStoreCompleted(int token, qint64 insertIdOrNumRowsAffected, int error, QString errorDescription);
    void onLengthChanged();
//    void onVodEndAvailable(qint64 rowid, int endOffsetS);

private:
    ScVodDataManager* manager() const;
    bool fetch(Data* data) const;
    void setSeen(bool seen);
    void setSeenMember(bool value);

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
    int m_season;
    int m_year;
    int m_match_number;
    int m_stage_rank;
    bool m_seen;
};
