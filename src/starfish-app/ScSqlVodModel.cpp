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

#include "ScSqlVodModel.h"
#include "ScStopwatch.h"
#include <QSqlQuery>
#include <QSqlError>
#include <QDebug>


// https://wiki.qt.io/How_to_Use_a_QSqlQueryModel_in_QML

ScSqlVodModel::ScSqlVodModel(QObject* parent)
    : QSqlQueryModel(parent)
{
    m_Dirty = false;
}

QVariant
ScSqlVodModel::data(const QModelIndex &modelIndex, int role) const
{
    if (role < Qt::UserRole) {
        return QSqlQueryModel::data(modelIndex, role);
    }

    int column = role - Qt::UserRole - 1;
    QModelIndex remappedIndex = index(modelIndex.row(), column);
    return QSqlQueryModel::data(remappedIndex, Qt::DisplayRole);
}

void
ScSqlVodModel::setSelect(QString newValue)
{
    if (m_Select != newValue) {
        m_Select = newValue;
        m_Dirty = true;
        update();
        emit selectChanged();
    }
}

void
ScSqlVodModel::setDatabase(const QVariant& newValue)
{
    m_Dirty = true;
    m_Database = qvariant_cast<QSqlDatabase>(newValue);
    update();
    emit databaseChanged();
}


void
ScSqlVodModel::update()
{
    ScStopwatch sw("update", 10);
    if (m_Dirty) {
        m_Dirty = !tryConfigureModel();
    } else {
        setQuery();
    }
}

bool
ScSqlVodModel::tryConfigureModel()
{
    if (m_Database.isValid() &&
            m_Database.isOpen() &&
            !m_Select.isEmpty()) {

        setQuery();
        beginResetModel();
        for (int i = 0; i < m_Columns.count(); ++i) {
            setHeaderData(0, Qt::Horizontal, m_ColumnAliases.value(m_Columns[i], m_Columns[i]).toString());
        }
        endResetModel();

        return true;
    }

    return false;
}

void
ScSqlVodModel::setColumns(const QStringList& newValue)
{
    m_Dirty = true;
    m_Columns = newValue;
    updateColumns();
    emit columnsChanged();
}

void
ScSqlVodModel::setColumnAliases(const QVariantMap& newValue)
{
    m_Dirty = true;
    m_ColumnAliases = newValue;
    updateColumns();
    emit columnAliasesChanged();
}

void
ScSqlVodModel::updateColumns()
{
    m_RoleNames.clear();
    for (int i = 0; i < m_Columns.count(); ++i) {
        m_RoleNames[Qt::UserRole + i + 1] = m_ColumnAliases.value(m_Columns[i], m_Columns[i]).toString().toLocal8Bit();
    }

    update();
}

void
ScSqlVodModel::reload()
{
    update();
}

void
ScSqlVodModel::setQuery()
{
    QSqlQueryModel::setQuery(QSqlQuery(m_Select, m_Database));
    if (lastError().isValid()) {
        qDebug() << "model error" << lastError();
    }
}
