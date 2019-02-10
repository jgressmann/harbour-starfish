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
#include <functional>

#include "VMVod.h"

class ScVodDataManager;

class ScMatchItem : public QObject
{
    Q_OBJECT
    Q_PROPERTY(qint64 rowId READ rowId CONSTANT)
    Q_PROPERTY(bool seen READ seen WRITE setSeen NOTIFY seenChanged)
    Q_PROPERTY(bool metaDataAvailable READ metaDataAvailable NOTIFY metaDataAvailableChanged)
    Q_PROPERTY(QString thumbnailAvailable READ thumbnailAvailable NOTIFY thumbnailAvailableChanged)
    Q_PROPERTY(QString title READ title NOTIFY titleChanged)
    Q_PROPERTY(QString eventFullName READ eventFullName CONSTANT)
    Q_PROPERTY(QString eventName READ eventName CONSTANT)
    Q_PROPERTY(QString stageName READ stageName CONSTANT)
    Q_PROPERTY(QString side1 READ side1 CONSTANT)
    Q_PROPERTY(QString side2 READ side2 CONSTANT)
    Q_PROPERTY(int race1 READ race1 CONSTANT)
    Q_PROPERTY(int race2 READ race2 CONSTANT)
    Q_PROPERTY(QDate matchDate READ matchDate CONSTANT)
    Q_PROPERTY(QString filePath READ filePath NOTIFY filePathChanged)
    Q_PROPERTY(quint64 fileSize READ fileSize NOTIFY fileSizeChanged)
    Q_PROPERTY(int videoStartOffset READ videoStartOffset CONSTANT)
    Q_PROPERTY(int videoEndOffset READ videoEndOffset NOTIFY videoEndOffsetChanged)
    Q_PROPERTY(float downloadProgress READ downloadProgress NOTIFY downloadProgressChanged)
    Q_PROPERTY(int width READ width NOTIFY widthChanged)
    Q_PROPERTY(int height READ height NOTIFY heightChanged)
    Q_PROPERTY(int season READ season CONSTANT)
    Q_PROPERTY(int year READ year CONSTANT)
    Q_PROPERTY(int length READ length NOTIFY lengthChanged)
    Q_PROPERTY(QString url READ url CONSTANT)
public:
    enum Race
    {
        Unknown,
        Protoss,
        Random,
        Terran,
        Zerg
    };
    Q_ENUMS(Race)

public:
    explicit ScMatchItem(qint64 rowid, ScVodDataManager *parent);

public:
    qint64 rowId() const { return m_RowId; }
    bool seen() const { return m_seen; }
    bool metaDataAvailable() const { return m_MetaData.isValid(); }
    QString thumbnailAvailable() const { return m_ThumbnailPath; }
    QString eventFullName() const { return m_EventFullName; }
    QString eventName() const { return m_EventName; }
    QString stageName() const { return m_stageName; }
    QString side1() const { return m_side1; }
    QString side2() const { return m_side2; }
    int race1() const { return m_race1; }
    int race2() const { return m_race2; }
    QDate matchDate() const { return m_matchDate; }
    QString filePath() const { return m_FilePath; }
    qint64 fileSize() const { return m_FileSize; }
    int videoStartOffset() const { return m_VideoStartOffset; }
    int videoEndOffset() const { return m_VideoEndOffset; }
    float downloadProgress() const { return m_Progress; }
    int width() const { return m_Width; }
    int height() const { return m_Height; }
    int season() const { return m_season; }
    int year() const { return m_year; }
    QString title() const { return m_Title; }
    int length() const { return m_Length; }
    QString url() const { return m_Url; }

signals:
    void seenChanged();
    void metaDataAvailableChanged();
    void thumbnailAvailableChanged();
    void filePathChanged();
    void fileSizeChanged();
    void videoEndOffsetChanged();
    void downloadProgressChanged();
    void widthChanged();
    void heightChanged();
    void startProcessDatabaseStoreQueue(int transactionId, QString sql, QVariantList args);
    void titleChanged();
    void lengthChanged();

private:
    struct Data
    {
        QString title;
        int length;
        bool seen;

        Data() = default;
    };

    using DatabaseCallback = std::function<void(qint64 insertIdOrNumRows, bool error)>;

private slots:
    void reset();
    void onSeenChanged();
    void onTitleAvailable(qint64 rowid, QString title);
    void onSeenAvailable(qint64 rowid, qreal seen);
    void onVodEndAvailable(qint64 rowid, int endOffsetS);
    void databaseStoreCompleted(int token, qint64 insertIdOrNumRowsAffected, int error, QString errorDescription);

private:
    ScVodDataManager* manager() const;
    bool fetch(Data* data) const;
    void setSeen(bool seen);

private:
    QHash<int, DatabaseCallback> m_PendingDatabaseStores;
    VMVod m_MetaData;
    QString m_EventFullName;
    QString m_EventName;
    QString m_stageName;
    QString m_matchName;
    QString m_side1;
    QString m_side2;
    QString m_ThumbnailPath;
    QString m_FilePath;
    QString m_Title;
    QString m_Url;
    qint64 m_RowId;
    qint64 m_FileSize;
    QDate m_matchDate;
    int m_race1;
    int m_race2;
    int m_VideoStartOffset;
    int m_VideoEndOffset;
    int m_season;
    int m_year;
    int m_match_number;
    int m_stage_rank;
    int m_Width;
    int m_Height;
    int m_Length;
    float m_Progress;
    bool m_seen;
};
