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

#include "ScApp.h"
#include "ScDatabaseStoreQueueTypes.h"

#include <QAbstractListModel>

//class ScRecentlyWatchedVideoKey
//{
//    Q_GADGET
//    Q_PROPERTY(QString filePath READ filePath CONSTANT)
//    Q_PROPERTY(qint64 vodId READ vodId CONSTANT)

//public:
//    ~ScRecentlyWatchedVideoKey() noexcept = default;
//    ScRecentlyWatchedVideoKey() noexcept : m_VodId(-1) {}
//    ScRecentlyWatchedVideoKey(qint64 value) noexcept : m_VodId(value) {}
//    ScRecentlyWatchedVideoKey(const QString& value) noexcept : m_FilePath(value), m_VodId(-1) {}
//    ScRecentlyWatchedVideoKey(const ScRecentlyWatchedVideoKey& /*other*/) noexcept = default;
//    ScRecentlyWatchedVideoKey& operator=(const ScRecentlyWatchedVideoKey& /*other*/) noexcept = default;
//    QString filePath() const noexcept { return m_FilePath; }
////    void setFilePath(const QString& value) noexcept { m_FilePath = value; }
//    qint64 vodId() const noexcept { return m_VodId; }
////    void setVodId(qint64 value) noexcept { m_VodId = value; }
//    bool isValid() const { return m_VodId >= 0 || !m_FilePath.isEmpty(); }
//    bool isVod() const { return m_VodId >= 0; }
//    bool isFile() const { return !m_FilePath.isEmpty(); }
//private:
//    QString m_FilePath;
//    qint64 m_VodId;
//};

//Q_DECLARE_METATYPE(ScRecentlyWatchedVideoKey)

class ScVodDataManager;
class ScRecentlyWatchedVideos : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(int count READ count WRITE setCount NOTIFY countChanged)

    using DatabaseCallback = std::function<void(qint64 insertIdOrNumRows, bool error)>;
    enum KeyType {
        Vod,
        Url
    };

public:
    explicit ScRecentlyWatchedVideos(ScVodDataManager* parent = Q_NULLPTR);

public: // QAbstractListModel
    Q_INVOKABLE virtual int rowCount(const QModelIndex &parent = QModelIndex()) const Q_DECL_OVERRIDE;
    Q_INVOKABLE virtual QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const Q_DECL_OVERRIDE;
    virtual QHash<int,QByteArray> roleNames() const Q_DECL_OVERRIDE;

public:
    int count() const { return m_Count; }
    void setCount(int value);
    Q_INVOKABLE QVariant urlKey(const QString& url) { return QVariant(url); }
    Q_INVOKABLE QVariant vodKey(qint64 rowid) { return QVariant(rowid); }
    Q_INVOKABLE void add(const QVariant& key, bool seen);
    Q_INVOKABLE void remove(const QVariant& key);
    Q_INVOKABLE void setOffset(const QVariant& key, int offset);
    Q_INVOKABLE int offset(const QVariant& key) const;
    Q_INVOKABLE void setThumbnailPath(const QVariant& key, const QString& thumbnailPath);
    Q_INVOKABLE void clear();

public slots:
    void reset();

signals:
    void databaseChanged();
    void countChanged();
    void startProcessDatabaseStoreQueue(int transactionId, QString sql, ScSqlParamList args);

private slots:
    void databaseStoreCompleted(int token, qint64 insertIdOrNumRowsAffected, int error, QString errorDescription);

private:
    ScVodDataManager* manager() const;
    void deleteExistingThumbnailFile(const QVariant& key) const;
    static bool keyType(const QVariant& v, KeyType* keytype);

private:
    QHash<int, DatabaseCallback> m_PendingDatabaseStores;
    mutable QVector<QVariant> m_RowCache;
    int m_Count;
    mutable int m_RowCount;
    mutable int m_IndexCache;

private:
    static const QHash<int, QByteArray> ms_RoleNames;
};

