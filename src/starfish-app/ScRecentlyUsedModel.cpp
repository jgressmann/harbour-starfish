#include "ScRecentlyUsedModel.h"


#include <QSqlQuery>
#include <QSqlError>
#include <QDebug>
#include <QDateTime>

ScRecentlyUsedModel::ScRecentlyUsedModel(QObject* parent)
    : QAbstractTableModel(parent)
{
//    m_Db = nullptr;
    m_Count = 10;
//    m_NeedToCreateTable = true;
    m_Dirty = false;
    m_Ready = false;
    m_IndexCache = -1;
    m_RowCount = 0;
}
void
ScRecentlyUsedModel::setDatabase(const QVariant& value) {
    auto db = qvariant_cast<QSqlDatabase>(value);

    if (m_Ready && m_Dirty) {
        if (writeBackCache()) {
            m_Dirty = false;
            m_IndexCache = -1;
        } else {
            qCritical().noquote().nospace() << "failed to write back cached row " << m_RowCache.last();
            return;
        }
    }

    m_Db = db;
//    m_NeedToCreateTable = true;
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
        if (m_Ready && m_Dirty && value < m_Count && m_IndexCache >= value) {
            if (writeBackCache()) {
                m_Dirty = false;
                m_IndexCache = -1;
            } else {
                qCritical().noquote().nospace() << "failed to write back cached row " << m_RowCache.last();
                return;
            }
        }

        m_Count = value;
        tryGetReady();
        emit countChanged();
    }
}

void
ScRecentlyUsedModel::setTable(const QString& value) {
    if (value != m_Table) {
        if (m_Ready && m_Dirty) {
            if (writeBackCache()) {
                m_Dirty = false;
                m_IndexCache = -1;
            } else {
                qCritical().noquote().nospace() << "failed to write back cached row " << m_RowCache.last();
                return;
            }
        }

        m_Table = value;
//        m_NeedToCreateTable = true;
        m_RowCountSql = QStringLiteral("SELECT COUNT(*) FROM %1").arg(m_Table);
        m_SelectDataSql = QStringLiteral("SELECT * FROM %1 ORDER BY modified DESC").arg(m_Table);
        m_SelectOldestRowSql = QStringLiteral("SELECT id FROM %1 ORDER BY modified DESC LIMIT 1").arg(m_Table);
//        m_UpdateModifiedSql = QStringLiteral("UPDATE %1 SET modified=? WHERE id=?").arg(m_Table);
        m_DeleteRowSql = QStringLiteral("DELETE FROM %1 WHERE id=?").arg(m_Table);
        setColumnBasedSql();
        tryGetReady();
        emit tableChanged();
    }
}

void
ScRecentlyUsedModel::setColumnBasedSql() {
    m_UpdateRowSql = QStringLiteral("UPDATE %1 SET ").arg(m_Table);
    m_InsertRowSql = QStringLiteral("INSERT INTO %1 (").arg(m_Table);
    auto insertTail = QStringLiteral(", modified) VALUES (");
    for (int i = 0; i < m_ColumnNames.size(); ++i) {
        if (i > 0) {
            m_UpdateRowSql += QStringLiteral(", ");
            m_InsertRowSql += QStringLiteral(", ");
            insertTail += QStringLiteral(", ");
        }

        m_UpdateRowSql += QStringLiteral("%1=?").arg(m_ColumnNames[i]);
        m_InsertRowSql += m_ColumnNames[i];
        insertTail += QStringLiteral("?");
    }

    m_UpdateRowSql += QStringLiteral(", modified=? WHERE id=?");
    m_InsertRowSql += insertTail;
    m_InsertRowSql += QStringLiteral(", ?)");
}

void
ScRecentlyUsedModel::setColumnNames(const QStringList& value) {
    if (m_Ready && m_Dirty) {
        if (writeBackCache()) {
            m_Dirty = false;
            m_IndexCache = -1;
        } else {
            qCritical().noquote().nospace() << "failed to write back cached row " << m_RowCache.last();
            return;
        }
    }

    m_ColumnNames = value;
//    m_NeedToCreateTable = true;
    m_RowCache.resize(m_ColumnNames.size()+ExtraColumns);
    m_RoleNames.clear();
    for (int i = 0; i < m_ColumnNames.size(); ++i) {
        m_RoleNames[Qt::UserRole + i] = m_ColumnNames[i].toLocal8Bit();
    }
    m_RoleNames[Qt::UserRole + m_ColumnNames.size() + 0] = "modified";
    m_RoleNames[Qt::UserRole + m_ColumnNames.size() + 1] = "rowid";
    setColumnBasedSql();
    tryGetReady();
    emit columnNamesChanged();
}

void
ScRecentlyUsedModel::setColumnTypes(const QStringList& value) {
    if (m_Ready && m_Dirty) {
        if (writeBackCache()) {
            m_Dirty = false;
            m_IndexCache = -1;
        } else {
            qCritical().noquote().nospace() << "failed to write back cached row " << m_RowCache.last();
            return;
        }
    }

    m_ColumnTypes = value;
//    m_NeedToCreateTable = true;
    tryGetReady();
    emit columnNamesChanged();
}

void
ScRecentlyUsedModel::setRowIdentifierColumns(const QStringList& value) {
    if (m_Ready && m_Dirty) {
        if (writeBackCache()) {
            m_Dirty = false;
            m_IndexCache = -1;
        } else {
            qCritical().noquote().nospace() << "failed to write back cached row " << m_RowCache.last();
            return;
        }
    }

    m_RowIdentifierColumns = value;
//    m_NeedToCreateTable = true;
    tryGetReady();
    emit columnNamesChanged();
}

bool
ScRecentlyUsedModel::ready() const {
    return  !m_Table.isEmpty() &&
            !m_ColumnNames.isEmpty() &&
            m_ColumnNames.size() == m_ColumnTypes.size() &&
            !m_RowIdentifierColumns.isEmpty() &&
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
                if (m_IndexCache != -1) {
                    if (m_Dirty) {
                        if (writeBackCache()) {
                            m_Dirty = false;
                            m_IndexCache = -1;
                        } else {
                            return QVariant();
                        }
                    }
                }

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

//            m_Dirty = true;
//            m_RowCache[m_ColumnNames.size()] = QDateTime::currentDateTime();

            return m_RowCache[column];
        } else {
            qWarning().nospace().noquote() << "data: not ready";
        }
    }

    return QVariant();
}

bool
ScRecentlyUsedModel::writeBackCache() const {
    QSqlQuery q(m_Db);
    // write back date time update
    if (q.prepare(m_UpdateRowSql)) {
        for (int i = 0; i < m_RowCache.size(); ++i) {
            q.addBindValue(m_RowCache[i]);
        }
        if (q.exec()) {
            return true;
        } else {
            qWarning().nospace().noquote() << "failed to write back time stamp for row " << m_RowCache[m_RowCache.size()-1].toInt() << ", sql: " << m_UpdateRowSql << ", error: " << q.lastError();
            return false;
        }
    } else {
        qWarning().nospace().noquote() << "failed to write back time stamp for row " << m_RowCache[m_RowCache.size()-1].toInt() << ", sql: " << m_UpdateRowSql << ", error: " << q.lastError();
        return false;
    }
}

//bool
//ScRecentlyUsedModel::setData(const QModelIndex &index, const QVariant &value, int role) {
//    if (index.row() >= 0 && index.column() >= 0 && index.column() < m_ColumnNames.size()) {
//        auto r = index.row();
//        auto c = index.column();
//        if (m_Ready) {
//            if (m_IndexCache != r) {
//                if (m_IndexCache != -1) {
//                    if (m_Dirty) {
//                        if (writeBackCache()) {
//                            m_Dirty = false;
//                            m_IndexCache = -1;
//                        } else {
//                            return false;
//                        }
//                    }
//                }

//                QSqlQuery q(m_Db);
//                if (q.exec(m_SelectDataSql)) {
//                    auto r = index.row();
//                    for (int i = 0; i <= r; ++i) {
//                        if (!q.next()) {
//                            qWarning().nospace().noquote() << "failed to position on row " << r << ", error: " << q.lastError();
//                            return false;
//                        }
//                    }

//                    m_IndexCache = r;

//                    for (int i = 0; i < m_RowCache.size(); ++i) {
//                        m_RowCache[i] = q.value(i);
//                    }
//                } else {
//                    qWarning().nospace().noquote() << "failed select rows, sql: " << m_SelectDataSql << ", error: " << q.lastError();
//                    return false;
//                }
//            }

//            m_Dirty = true;
//            m_RowCache[m_ColumnNames.size()] = QDateTime::currentDateTime();
//            m_RowCache[c] = value;
//            return true;
//        }
//    }

//    return false;
//}

//Qt::ItemFlags
//ScRecentlyUsedModel::flags(const QModelIndex &index) const {
//    auto f = QAbstractTableModel::flags(index);
//    if (index.column() >= 0 && index.column() < m_ColumnNames.size()) {
//        f |= Qt::ItemIsEditable;
//    }

//    return f;
//}

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
            } else {
                qWarning().nospace().noquote() << "failed to exec delete, error: " << q.lastError();
            }
            endResetModel();
        }   else {
            qWarning().nospace().noquote() << "failed to prepare select sql: " << m_SelectRowsByKeySql << ", error: " << q.lastError();
        }
    } else {
        qWarning().noquote().nospace() << "remove: not ready";
    }
}

void
ScRecentlyUsedModel::add(const QList<QVariant>& values) {
    if (m_Ready) {
        if (values.size() != m_ColumnNames.size()) {
            qWarning().noquote().nospace() << "add: insufficient values";
        } else {
            QSqlQuery q(m_Db);
            // try to find the row
            qint64 rowid = -1;

            if (q.prepare(m_SelectRowsByKeySql)) {
                for (int i = 0; i < m_RowIdentifierColumns.size(); ++i) {

                    auto rowIdColumn = m_RowIdentifierColumns[i];
                    auto index = m_ColumnNames.indexOf(rowIdColumn);
                    if (index == -1) {
                        qWarning().noquote().nospace() << "add: invalid row identifier column " << rowIdColumn;
                        return;
                    }

                    q.addBindValue(values[index]);
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
                qWarning().nospace().noquote() << "failed to prepare select sql: " << m_SelectRowsByKeySql << ", error: " << q.lastError();
                return;
            }


//            for (int i = 0; i < m_RowIdentifierColumns.size(); ++i) {
//                auto rowIdColumn = m_RowIdentifierColumns[i];
//                auto index = m_ColumnNames.indexOf(rowIdColumn);
//                if (index == -1) {
//                    qWarning().noquote().nospace() << "add: invalid row identifier column " << rowIdColumn;
//                    return;
//                }

//                rowid = -1;
//                auto sql = QStringLiteral("SELECT id FROM %1 WHERE %2=?").arg(m_Table, rowIdColumn);
//                if (q.prepare(sql)) {
//                    q.addBindValue(values[index]);
//                    if (q.exec()) {
//                        if (q.next()) {
//                            rowid = qvariant_cast<qint64>(q.value(0));
//                            if (q.next()) {
//                                //rowid = -1; // must be a single row
//                                qDebug().noquote().nospace() << "single row with " << m_RowIdentifierColumns[i] << "=" << values[index];
//                            } else {
//                                qDebug().noquote().nospace() << "single row with " << m_RowIdentifierColumns[i] << "=" << values[index];
//                            }
//                        } else {
//                            qDebug().noquote().nospace() << "no row with " << m_RowIdentifierColumns[i] << "=" << values[index];
//                            break;
//                        }
//                    } else {
//                        qWarning().nospace().noquote() << "failed to exec select, error: " << q.lastError();
//                        return;
//                    }
//                } else {
//                    qWarning().nospace().noquote() << "failed to prepare select sql: " << sql << ", error: " << q.lastError();
//                    return;
//                }
//            }

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
                        --m_RowCount;
                        qDebug().nospace().noquote() << "delete rowid " << rowid;
                    } else {
                        qWarning().nospace().noquote() << "failed to exec insert, error: " << q.lastError();
                    }
                } else {
                    qWarning().nospace().noquote() << "failed to prepare insert, sql: " << m_InsertRowSql << ", error: " << q.lastError();
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
            if (q.prepare(m_InsertRowSql)) {
                for (int i = 0; i < values.size(); ++i) {
                    qDebug().nospace().noquote() << "\t" << values[i] << "\n";
                    q.addBindValue(values[i]);
                }

                q.addBindValue(QDateTime::currentDateTime());

                if (q.exec()) {
                    ++m_RowCount;
                    qDebug().nospace().noquote() << "new row, count " << m_RowCount;
                } else {
                    qWarning().nospace().noquote() << "failed to exec insert, error: " << q.lastError();
                }
            } else {
                qWarning().nospace().noquote() << "failed to prepare insert, sql: " << m_InsertRowSql << ", error: " << q.lastError();
            }
            emit endInsertRows();

        }
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
            for (int i = 0; i < m_RowIdentifierColumns.size(); ++i) {
                if (i > 0) {
                    m_SelectRowsByKeySql += QStringLiteral(" AND ");
                }

                m_SelectRowsByKeySql += QStringLiteral("(");
                m_SelectRowsByKeySql += m_RowIdentifierColumns[i];
                m_SelectRowsByKeySql += QStringLiteral(" IS NULL OR ");
                m_SelectRowsByKeySql += m_RowIdentifierColumns[i];
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
