/* The MIT License (MIT)
 *
 * Copyright (c) 2018, 2019 Jean Gressmann <jean@0x42.de>
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

#include <QSqlQueryModel>
#include <QList>
#include <QSqlDatabase>
#include <QVariant>

#include "ScApp.h" // meta type for QSqlDatabase


class ScSqlVodModel : public QSqlQueryModel
{
    Q_OBJECT
    Q_PROPERTY(QString select READ select WRITE setSelect NOTIFY selectChanged)
    Q_PROPERTY(QStringList columns READ columns WRITE setColumns NOTIFY columnsChanged)
    Q_PROPERTY(QVariantMap columnAliases READ columnAliases WRITE setColumnAliases NOTIFY columnAliasesChanged)
    Q_PROPERTY(QVariant database READ database WRITE setDatabase NOTIFY databaseChanged)

public:
    ~ScSqlVodModel() = default;
    explicit ScSqlVodModel(QObject* parent = Q_NULLPTR);

public:
    QString select() const { return m_Select; }
    void setSelect(QString newValue);
    QStringList columns() const { return m_Columns; }
    void setColumns(const QStringList& newValue);
    QVariantMap columnAliases() const { return m_ColumnAliases; }
    void setColumnAliases(const QVariantMap& newValue);
    QVariant database() const { return QVariant::fromValue(m_Database); }
    void setDatabase(const QVariant& newValue);
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const Q_DECL_OVERRIDE;
    QHash<int,QByteArray> roleNames() const Q_DECL_OVERRIDE { return m_RoleNames; }

signals:
    void selectChanged();
    void dataManagerChanged();
    void columnsChanged();
    void columnAliasesChanged();
    void databaseChanged();

public slots:
    // update is already used extensively in QML
    Q_INVOKABLE void reload();

private:
    bool tryConfigureModel();
    void updateColumns();
    void update();
    void setQuery();

private:
    QSqlDatabase m_Database;
    QStringList m_Columns;
    QVariantMap m_ColumnAliases;
    QHash<int, QByteArray> m_RoleNames;
    QString m_Select;
    bool m_Dirty;
};


