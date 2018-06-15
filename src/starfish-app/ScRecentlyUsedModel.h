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

#pragma once

#include "ScApp.h"

#include <QAbstractListModel>



class ScRecentlyUsedModel : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(QString table READ table WRITE setTable NOTIFY tableChanged)
    Q_PROPERTY(QStringList columnNames READ columnNames WRITE setColumnNames NOTIFY columnNamesChanged)
    Q_PROPERTY(QStringList columnTypes READ columnTypes WRITE setColumnTypes NOTIFY columnTypesChanged)
    Q_PROPERTY(QStringList keyColumns READ keyColumns WRITE setKeyColumns NOTIFY keyColumnsChanged)
    Q_PROPERTY(QVariant database READ database WRITE setDatabase NOTIFY databaseChanged)
    Q_PROPERTY(int count READ count WRITE setCount NOTIFY countChanged)
    Q_PROPERTY(bool changeForcesReset READ changeForcesReset WRITE setChangeForcesReset NOTIFY changeForcesResetChanged)
public:
    explicit ScRecentlyUsedModel(QObject* parent = Q_NULLPTR);

public: //
    Q_INVOKABLE virtual int rowCount(const QModelIndex &parent = QModelIndex()) const Q_DECL_OVERRIDE;
//    Q_INVOKABLE virtual int columnCount(const QModelIndex &parent = QModelIndex()) const Q_DECL_OVERRIDE;
    Q_INVOKABLE virtual QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const Q_DECL_OVERRIDE;
    virtual QHash<int,QByteArray> roleNames() const Q_DECL_OVERRIDE;
//    virtual Qt::ItemFlags flags(const QModelIndex &index) const Q_DECL_OVERRIDE;

public:
    QString table() const { return m_Table; }
    QStringList columnNames() const { return m_ColumnNames; }
    QStringList columnTypes() const { return m_ColumnTypes; }
    QStringList keyColumns() const { return m_KeyColumns; }
    QVariant database() const { return QVariant::fromValue(m_Db); }
    int count() const { return m_Count; }
    bool changeForcesReset() const { return m_ChangeForcesReset; }
    void setDatabase(const QVariant& value);
    void setCount(int value);
    void setTable(const QString& value);
    void setColumnNames(const QStringList& value);
    void setColumnTypes(const QStringList& value);
    void setKeyColumns(const QStringList& value);
    void setChangeForcesReset(bool value);
    Q_INVOKABLE void add(const QVariantMap& pairs);
    Q_INVOKABLE void update(const QVariantMap& values, const QVariantMap& where);
    Q_INVOKABLE void remove(const QVariantMap& where);
    Q_INVOKABLE QVariantList select(const QStringList& columns, const QVariantMap& where);
    Q_INVOKABLE void recreateTable();

signals:
    void databaseChanged();
    void countChanged();
    void tableChanged();
    void columnNamesChanged();
    void columnTypesChanged();
    void keyColumnsChanged();
    void changeForcesResetChanged();
private:
    enum {
        ExtraColumns = 2,
    };
private:
    bool ready() const;
    bool createTable();
    void tryGetReady();

private:
    QStringList m_ColumnNames, m_ColumnTypes, m_KeyColumns;
    mutable QVector<QVariant> m_RowCache;
    QHash<int,QByteArray> m_RoleNames;
    QString m_Table;
    QSqlDatabase m_Db;
    int m_Count;
    int m_RowCount;
    mutable int m_IndexCache;
    bool m_Ready;
    bool m_ChangeForcesReset;
};

