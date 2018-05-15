#include "ScRecentlyUsedModel.h"


#include <QSqlQuery>
#include <QSqlError>
#include <QDebug>
#include <QDateTime>

ScRecentlyUsedModel::ScRecentlyUsedModel(QObject* parent)
    : QAbstractTableModel(parent)
{
    m_Count = 10;
    m_Ready = false;
    m_IndexCache = -1;
    m_RowCount = 0;
}
void
ScRecentlyUsedModel::setDatabase(const QVariant& value) {
    m_Db = qvariant_cast<QSqlDatabase>(value);;

    tryGetReady();
    emit databaseChanged();
}

void
ScRecentlyUsedModel::setCount(int value) {
    if (value <= 0) {
        qWarning().nospace() << "count must be greater than 0" << value;
        return;
    }

    if (value != m_Count) {
        m_Count = value;
        tryGetReady();
        emit countChanged();
    }
}

void
ScRecentlyUsedModel::setTable(const QString& value) {
    if (value != m_Table) {

        m_Table = value;
        m_RowCountSql = QStringLiteral("SELECT COUNT(*) FROM %1").arg(m_Table);
        m_SelectDataSql = QStringLiteral("SELECT * FROM %1 ORDER BY modified DESC").arg(m_Table);
        m_DeleteRowSql = QStringLiteral("DELETE FROM %1 WHERE id=?").arg(m_Table);
        tryGetReady();
        emit tableChanged();
    }
}

void
ScRecentlyUsedModel::setColumnNames(const QStringList& value) {

    m_ColumnNames = value;
    m_RowCache.resize(m_ColumnNames.size()+ExtraColumns);
    m_RoleNames.clear();
    for (int i = 0; i < m_ColumnNames.size(); ++i) {
        m_RoleNames[Qt::UserRole + i] = m_ColumnNames[i].toLocal8Bit();
    }
    m_RoleNames[Qt::UserRole + m_ColumnNames.size() + 0] = "modified";
    m_RoleNames[Qt::UserRole + m_ColumnNames.size() + 1] = "rowid";
    tryGetReady();
    emit columnNamesChanged();
}

void
ScRecentlyUsedModel::setColumnTypes(const QStringList& value) {

    m_ColumnTypes = value;
    tryGetReady();
    emit columnNamesChanged();
}

void
ScRecentlyUsedModel::setRowIdentifierColumns(const QStringList& value) {
    m_KeyColumns = value;
    tryGetReady();
    emit columnNamesChanged();
}

bool
ScRecentlyUsedModel::ready() const {
    return  !m_Table.isEmpty() &&
            !m_ColumnNames.isEmpty() &&
            m_ColumnNames.size() == m_ColumnTypes.size() &&
            !m_KeyColumns.isEmpty() &&
            m_Db.isOpen();
}

int
ScRecentlyUsedModel::rowCount(const QModelIndex &/*parent*/) const {
    return m_RowCount;
}

int
ScRecentlyUsedModel::columnCount(const QModelIndex &/*parent*/) const {
    return 1;
}

QVariant
ScRecentlyUsedModel::data(const QModelIndex &index, int role) const {

    int column = Qt::DisplayRole == role ? index.column() : role - Qt::UserRole;
    auto row = index.row();
    if (row >= 0 && row < m_RowCount && column >= 0 && column < m_ColumnNames.size() + ExtraColumns) {
        if (m_Ready) {
            if (m_IndexCache != row) {
                QSqlQuery q(m_Db);
                if (q.exec(m_SelectDataSql)) {
                    for (int i = 0; i <= row; ++i) {
                        if (!q.next()) {
                            qWarning().nospace().noquote() << "failed to position on row " << row << ", error: " << q.lastError();
                            return QVariant();
                        }
                    }

                    m_IndexCache = row;

                    for (int i = 0; i < m_RowCache.size(); ++i) {
                        m_RowCache[i] = q.value(i);
                    }
                } else {
                    qWarning().nospace().noquote() << "failed select rows, sql: " << m_SelectDataSql << ", error: " << q.lastError();
                    return QVariant();
                }
            }

            return m_RowCache[column];
        } else {
            qWarning().nospace().noquote() << "data: not ready";
        }
    }

    return QVariant();
}

void
ScRecentlyUsedModel::remove(const QVariantMap& pairs) {
    if (pairs.empty()) {
        qWarning().noquote().nospace() << "remove: no keys";
        return;
    }

    if (m_Ready) {
        auto sql = QStringLiteral("DELETE FROM %1 WHERE ").arg(m_Table);
        auto beg = pairs.cbegin();
        auto end = pairs.cend();
        for (auto it = beg; it != end; ++it) {
            if (it != beg) {
                sql += QStringLiteral(" AND ");
            }

            sql += QStringLiteral("(%1 IS NULL OR %1=?)").arg(it.key());
        }

        QSqlQuery q(m_Db);
        if (q.prepare(sql)) {
            for (auto it = beg; it != end; ++it) {
                q.addBindValue(it.value());
            }

            beginResetModel();
            if (q.exec()) {
                qInfo().nospace().noquote() << "deleted";
                m_RowCount = qMax(0, m_RowCount-q.numRowsAffected());
                m_IndexCache = -1;
            } else {
                qWarning().nospace().noquote() << "failed to exec delete, error: " << q.lastError();
            }
            endResetModel();
        }   else {
            qWarning().nospace().noquote() << "failed to prepare select sql: " << sql << ", error: " << q.lastError();
        }
    } else {
        qWarning().noquote().nospace() << "remove: not ready";
    }
}


void
ScRecentlyUsedModel::update(const QVariantMap& keys, const QVariantMap& values) {
    if (keys.empty()) {
        qWarning().noquote().nospace() << "update: no keys";
        return;
    }

    if (values.empty()) {
        qWarning().noquote().nospace() << "update: no values";
        return;
    }
    if (m_Ready) {

    } else {
        qWarning().noquote().nospace() << "update: not ready";
    }
}

void
ScRecentlyUsedModel::add(const QVariantMap& pairs) {
    if (m_Ready) {
        QList<int> identifierIndices;
        auto insertRowSql = QStringLiteral("INSERT INTO %1 (").arg(m_Table);
        {
            auto insertRowSqlTail = QStringLiteral(", modified) VALUES (?, ");
            auto beg = pairs.cbegin();
            auto end = pairs.cend();
            for (auto it = beg; it != end; ++it) {
                auto index = m_KeyColumns.indexOf(it.key());
                if (index >= 0) {
                    identifierIndices << index;
                }

                if (it != beg) {
                    insertRowSql += QStringLiteral(", ");
                    insertRowSqlTail += QStringLiteral(", ");
                }

                insertRowSql += it.key();
                insertRowSqlTail += QStringLiteral("?");
            }

            insertRowSql += insertRowSqlTail;
            insertRowSql += QStringLiteral(")");
        }

        if (identifierIndices.empty()) {
            qWarning().noquote().nospace() << "add: no identifiers found";
            return;
        }

        auto selectRowSql = QStringLiteral("SELECT id FROM %1 WHERE ").arg(m_Table);
        for (int i = 0; i < identifierIndices.size(); ++i) {
            if (i > 0) {
                selectRowSql += QStringLiteral(" AND ");
            }

            auto index = identifierIndices[i];
//            selectRowSql += QStringLiteral("(");
//            selectRowSql += m_KeyColumns[index];
//            selectRowSql += QStringLiteral(" IS NULL OR ");
//            selectRowSql += m_KeyColumns[index];
//            selectRowSql += QStringLiteral("=?)");
            selectRowSql += m_KeyColumns[index];
            selectRowSql += QStringLiteral("=?");
        }


            QSqlQuery q(m_Db);
            // try to find the row
            qint64 rowid = -1;

            if (q.prepare(selectRowSql)) {
                for (int i = 0; i < identifierIndices.size(); ++i) {
                    auto index = identifierIndices[i];
                    const auto& columnName = m_KeyColumns[index];
                    q.addBindValue(pairs[columnName]);
                }

                if (q.exec()) {
                    if (q.next()) {
                        rowid = qvariant_cast<qint64>(q.value(0));
//                        if (q.next()) {
//                            rowid = -1; // must be a single row
//                            qDebug().noquote().nospace() << "multiple rows";
//                        } else {
//                            qDebug().noquote().nospace() << "single row";
//                        }
                    } else {
                        qDebug().noquote().nospace() << "no row";
                    }
                } else {
                    qWarning().nospace().noquote() << "failed to exec select, error: " << q.lastError();
                    return;
                }
            }   else {
                qWarning().nospace().noquote() << "failed to prepare select sql: " << selectRowSql << ", error: " << q.lastError();
                return;
            }


            int rowIndex = -1;
            auto sorted = false;
            if (-1 == rowid && m_RowCount == m_Count) {
                // select last row
                auto index = createIndex(m_RowCount-1, m_RowCache.size()-1);
                rowid = qvariant_cast<qint64>(data(index, Qt::DisplayRole));
                rowIndex = m_RowCount-1;
                sorted = true;
            }

            if (-1 != rowid) {
                if (-1 == rowIndex) { // find index to remove row
                    for (int i = 0; i < m_RowCount; ++i) {
                        sorted = true;
                        auto index = createIndex(i, m_RowCache.size()-1);
                        auto value = data(index, Qt::DisplayRole);
                        auto id = qvariant_cast<qint64>(value);
                        if (id == rowid) {
                            rowIndex = i;
                            break;
                        }
                    }
                }

                if (-1 == rowIndex) { // find index to remove row
                    qWarning().nospace().noquote() << "failed to get row index for id " << rowid;
                    return;
                }

                beginRemoveRows(QModelIndex(), rowIndex, rowIndex);
                if (q.prepare(m_DeleteRowSql)) {
                    q.addBindValue(rowid);
                    if (q.exec()) {
                        m_IndexCache = -1;
                        --m_RowCount;
                        qDebug().nospace().noquote() << "delete rowid " << rowid;
                    } else {
                        qWarning().nospace().noquote() << "failed to exec insert, error: " << q.lastError();
                    }
                } else {
                    qWarning().nospace().noquote() << "failed to prepare insert, sql: " << m_DeleteRowSql << ", error: " << q.lastError();
                }
                endRemoveRows();

                if (m_RowCount == m_Count) {
                    return; //error
                }
            }

            // ensure data is sorted
            if (!sorted && m_RowCount > 1) {
                auto index = createIndex(0, 0);
                data(index, Qt::DisplayRole);
            }

            // newest row
            qDebug().nospace().noquote() << "insert";
            emit beginInsertRows(QModelIndex(), 0, 0);
            if (q.prepare(insertRowSql)) {
                auto beg = pairs.cbegin();
                auto end = pairs.cend();
                for (auto it = beg; it != end; ++it) {
                    qDebug().nospace().noquote() << "\t" << it.value() << "\n";
                    q.addBindValue(it.value());
                }

                q.addBindValue(QDateTime::currentDateTime());

                if (q.exec()) {
                    m_IndexCache = -1;
                    ++m_RowCount;
                    qDebug().nospace().noquote() << "new row, count " << m_RowCount;
                } else {
                    qWarning().nospace().noquote() << "failed to exec insert, error: " << q.lastError();
                }
            } else {
                qWarning().nospace().noquote() << "failed to prepare insert, sql: " << insertRowSql << ", error: " << q.lastError();
            }
            emit endInsertRows();


    } else {
        qWarning().noquote().nospace() << "add: not ready";
    }
}

bool
ScRecentlyUsedModel::createTable() {

    QSqlQuery q(m_Db);

    QString sql = QStringLiteral(
"CREATE TABLE IF NOT EXISTS %1 (\n"
                                 ).arg(m_Table);

    for (int i = 0; i < m_ColumnNames.size(); ++i) {
        if (i > 0) {
            sql += QStringLiteral(",\n");
        }
        sql += QStringLiteral("   %1 %2").arg(m_ColumnNames[i], m_ColumnTypes[i]);
    }

    sql += QStringLiteral(",\n   modified INTEGER NOT NULL,\n   id INTEGER PRIMARY KEY)\n");

    if (!q.exec(sql)) {
        qWarning().noquote().nospace() << "failed to create table using sql\n" << sql << "error: " << q.lastError();
        return false;
    }

    return true;
}

void
ScRecentlyUsedModel::tryGetReady() {
    m_RowCount = 0;
    m_Ready = false;
    if (ready()) {
        if (createTable()) {
            m_Ready = true;
            m_SelectRowsByKeySql = QStringLiteral("SELECT id FROM %1 WHERE ").arg(m_Table);
            for (int i = 0; i < m_KeyColumns.size(); ++i) {
                if (i > 0) {
                    m_SelectRowsByKeySql += QStringLiteral(" AND ");
                }

                m_SelectRowsByKeySql += QStringLiteral("(");
                m_SelectRowsByKeySql += m_KeyColumns[i];
                m_SelectRowsByKeySql += QStringLiteral(" IS NULL OR ");
                m_SelectRowsByKeySql += m_KeyColumns[i];
                m_SelectRowsByKeySql += QStringLiteral("=?)");
            }
        }
    }

    if (m_Ready) {
        QSqlQuery q(m_Db);
        if (q.exec(m_RowCountSql)) {
            if (q.next()) {
                m_RowCount = q.value(0).toInt();

                if (m_RowCount > m_Count) {
                    auto n = QString::number(m_RowCount-m_Count);
                    beginResetModel();
                    endResetModel();
                    beginRemoveRows(QModelIndex(), m_Count, m_RowCount-1);
                    auto sql = QStringLiteral("DELETE from %1 WHERE id IN (SELECT id from %1 ORDER BY modified DESC LIMIT %2").arg(m_Table, n);
                    if (q.exec(sql)) {
                        qInfo().noquote().nospace() << "deleted " << n << " extra rows";
                        m_RowCount = m_Count;
                    } else {
                        m_Ready = false;
                        qWarning().noquote().nospace() << "failed to delete " << n << " extra rows, error: " << q.lastError();
                    }
                    endRemoveRows();
                }
            } else {
                m_Ready = false;
                qWarning().noquote().nospace() << "failed to position on result, error: " << q.lastError();
            }
        } else {
            m_Ready = false;
            qWarning().noquote().nospace() << "failed to fetch row count using sql\n" << m_RowCountSql << ", error: " << q.lastError();
        }
    }

    if (m_Ready) {
        beginResetModel();
        endResetModel();
    }
}

void
ScRecentlyUsedModel::recreateTable() {
    if (m_Db.isOpen()) {
        QSqlQuery q(m_Db);
        auto sql = QStringLiteral("DROP TABLE IF EXISTS %1").arg(m_Table);
        if (q.exec(sql)) {
            qInfo().noquote().nospace() << "table " << m_Table << " droped";
            m_Ready = false;
//            m_NeedToCreateTable = true;
            tryGetReady();
        } else {
            qWarning().noquote().nospace() << "failed to drop table, sql\n" << sql << "error: " << q.lastError();
        }
    } else {
        qWarning().noquote().nospace() << "no database set";
    }
}

QHash<int,QByteArray>
ScRecentlyUsedModel::roleNames() const {
    return m_RoleNames;
}
