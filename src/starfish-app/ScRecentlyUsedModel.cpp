#include "ScRecentlyUsedModel.h"


#include <QSqlQuery>
#include <QSqlError>
#include <QDebug>
#include <QDateTime>

ScRecentlyUsedModel::ScRecentlyUsedModel(QObject* parent)
    : QAbstractListModel(parent)
{
    m_Count = 10;
    m_Ready = false;
    m_ChangeForcesReset = false;
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
ScRecentlyUsedModel::setKeyColumns(const QStringList& value) {
    m_KeyColumns = value;
    tryGetReady();
    emit columnNamesChanged();
}

void
ScRecentlyUsedModel::setChangeForcesReset(bool value) {
    if (value != m_ChangeForcesReset) {
        m_ChangeForcesReset = value;
        emit changeForcesResetChanged();
    }
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

//int
//ScRecentlyUsedModel::columnCount(const QModelIndex &/*parent*/) const {
//    return 1;
//}

QVariant
ScRecentlyUsedModel::data(const QModelIndex &index, int role) const {

    int column = Qt::DisplayRole == role ? index.column() : role - Qt::UserRole;
    auto row = index.row();
    if (row >= 0 && row < m_RowCount && column >= 0 && column < m_ColumnNames.size() + ExtraColumns) {

        if (m_Ready) {
            if (m_IndexCache != row) {
                QSqlQuery q(m_Db);
                auto sql = QStringLiteral("SELECT * from %1 ORDER BY modified DESC LIMIT %2, 1").arg(m_Table, QString::number(row));
                if (q.exec(sql)) {
                    if (q.next()) {
                        m_IndexCache = row;
                        for (int i = 0; i < m_RowCache.size(); ++i) {
                            m_RowCache[i] = q.value(i);
                        }
                    } else {
                        qWarning().nospace().noquote() << "failed get row " << row << ", error: " << q.lastError();
                        return QVariant();
                    }
                    for (int i = 0; i <= row; ++i) {
                        if (!q.next()) {

                        }
                    }
                } else {
                    qWarning().nospace().noquote() << "failed select row, sql: " << sql << ", error: " << q.lastError();
                    return QVariant();
                }
            }

            const auto& value = m_RowCache[column];
//            qDebug().nospace().noquote() << "row " << row << " column " << column << " " << value;
            return value;
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

            sql += QStringLiteral("%1=?").arg(it.key());
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

QVariantList
ScRecentlyUsedModel::select(const QStringList& columns, const QVariantMap& where) {
    const QStringList* cols = &columns;
    if (columns.empty()) {
        cols = &m_ColumnNames;
    }

    QVariantList result;

    if (m_Ready) {

        auto sql = QStringLiteral("SELECT ");
        {
            auto beg = cols->cbegin();
            auto end = cols->cend();
            for (auto it = beg; it != end; ++it) {
                if (it != beg) {
                    sql += QStringLiteral(", ");
                }

                sql += *it;
            }

            sql += QStringLiteral(" FROM %1").arg(m_Table);

            if (!where.isEmpty()) {
                sql += QStringLiteral(" WHERE ");

                auto beg = where.cbegin();
                auto end = where.cend();
                for (auto it = beg; it != end; ++it) {
                    if (it != beg) {
                        sql += QStringLiteral(" AND ");
                    }

                    sql += it.key();
                    sql += QStringLiteral("=?");
                }
            }
        }

        QSqlQuery q(m_Db);
        if (q.prepare(sql)) {
            if (!where.isEmpty()) {
                auto beg = where.cbegin();
                auto end = where.cend();
                for (auto it = beg; it != end; ++it) {
                    q.addBindValue(it.value());
                }
            }

            if (q.exec()) {
                while (q.next()) {
                    QVariantMap row;
                    for (int i = 0; i < cols->size(); ++i) {
                        row.insert((*cols)[i], q.value(i));
                    }

                    result << row;
                }
            } else {
                qWarning().nospace().noquote() << "failed to exec select, error: " << q.lastError();
            }
        }   else {
            qWarning().nospace().noquote() << "failed to prepare select sql: " << sql << ", error: " << q.lastError();
        }

    } else {
        qWarning().noquote().nospace() << "update: not ready";
    }

    return result;
}

void
ScRecentlyUsedModel::update(const QVariantMap& values, const QVariantMap& where) {
    if (where.empty()) {
        qWarning().noquote().nospace() << "update: no where clause";
        return;
    }

    if (values.empty()) {
        qWarning().noquote().nospace() << "update: no values";
        return;
    }
    if (m_Ready) {
//recentlyUsedVideos.update({ thumbnail_path: openVideo.thumbnailFilePath}, { id: openVideo.rowId})
        auto selectSql = QStringLiteral("SELECT id FROM %1").arg(m_Table);
        auto updateSql = QStringLiteral("UPDATE %1 SET ").arg(m_Table);
        QString updateSqlDebug;
        auto whereClauseDebug = QStringLiteral(" WHERE ");
        {
            auto beg = values.cbegin();
            auto end = values.cend();
            for (auto it = beg; it != end; ++it) {
                if (it != beg) {
                    updateSql += QStringLiteral(", ");
                    updateSqlDebug += QStringLiteral(" ");
                }

                auto index = m_ColumnNames.indexOf(it.key());
                if (index == -1) {
                    qWarning().noquote().nospace() << "update: unknown column " << it.key();
                    return;
                }

                updateSql += m_ColumnNames[index];
                updateSql += QStringLiteral("=?");

                updateSqlDebug += it.key();
                updateSqlDebug += QStringLiteral("=");
                updateSqlDebug += it.value().toString();
            }


//            updateSql += QStringLiteral(", modified=?");
            auto whereClause = QStringLiteral(" WHERE ");

            beg = where.cbegin();
            end = where.cend();
            for (auto it = beg; it != end; ++it) {
                if (it != beg) {
                    whereClause += QStringLiteral(" AND ");
                    whereClauseDebug += QStringLiteral(" AND ");
                }

                auto index = m_ColumnNames.indexOf(it.key());
                if (index == -1) {
                    qWarning().noquote().nospace() << "update: unknown column " << it.key();
                    return;
                }

                whereClause += m_ColumnNames[index];
                whereClause += QStringLiteral("=?");

                whereClauseDebug += m_ColumnNames[index];
                whereClauseDebug += QStringLiteral("=");
                whereClauseDebug += it.value().toString();

            }

            updateSql += whereClause;
            selectSql += whereClause;
        }

        QSqlQuery q(m_Db);
        if (q.prepare(updateSql)) {
            auto beg = values.cbegin();
            auto end = values.cend();
            for (auto it = beg; it != end; ++it) {
                q.addBindValue(it.value());
            }

//            q.addBindValue(QDateTime::currentDateTime());

            beg = where.cbegin();
            end = where.cend();
            for (auto it = beg; it != end; ++it) {
                q.addBindValue(it.value());
            }

            if (q.exec()) {
                qDebug().nospace().noquote() << "updated " << q.numRowsAffected() << " rows: " << updateSqlDebug << whereClauseDebug;
                m_IndexCache = -1;
                if (q.numRowsAffected()) {
                    if (m_ChangeForcesReset) {
                        beginResetModel();
                        endResetModel();
                    } else {
                        emit dataChanged(createIndex(0, 0), createIndex(m_RowCount-1, 0));
                    }
//                    emit dataChanged(createIndex(0, 0), createIndex(m_RowCount-1, 0));
                }
            } else {
                qWarning().nospace().noquote() << "failed to exec update, error: " << q.lastError();
            }
        }   else {
            qWarning().nospace().noquote() << "failed to prepare select sql: " << updateSql << ", error: " << q.lastError();
        }
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

//        auto selectRowSql = QStringLiteral("SELECT id FROM %1").arg(m_Table);
        auto rowWhereClause = QStringLiteral(" WHERE ");
        for (int i = 0; i < identifierIndices.size(); ++i) {
            if (i > 0) {
                rowWhereClause += QStringLiteral(" AND ");
            }

            auto index = identifierIndices[i];
            rowWhereClause += m_KeyColumns[index];
            rowWhereClause += QStringLiteral("=?");
        }

//        selectRowSql += rowWhereClause;


            QSqlQuery q(m_Db);

            m_IndexCache = -1;
            if (q.prepare(QStringLiteral("UPDATE %1 SET modified=? %2").arg(m_Table, rowWhereClause))) {

                q.addBindValue(QDateTime::currentDateTime());

                QString debugWhere;
                for (int i = 0; i < identifierIndices.size(); ++i) {
                    if (i > 0)  {
                        debugWhere += QStringLiteral(" AND ");
                    }
                    auto index = identifierIndices[i];
                    const auto& columnName = m_KeyColumns[index];
                    q.addBindValue(pairs[columnName]);
                    debugWhere += columnName;
                    debugWhere += QStringLiteral("=");
                    debugWhere += pairs[columnName].toString();
                }

                if (q.exec()) {
                    if (q.numRowsAffected() > 0) {
                        qDebug().nospace().noquote() << "updated modified " << q.numRowsAffected() << " rows " << debugWhere;
                        if (m_ChangeForcesReset) {
                            beginResetModel();
                            endResetModel();
                        } else {
                            emit dataChanged(createIndex(0, 0), createIndex(m_RowCount-1, 0));
                        }
                    } else {
                        if (m_RowCount == m_Count) {
                            beginRemoveRows(QModelIndex(), m_RowCount-1, m_RowCount-1);

                            if (q.exec(QStringLiteral("DELETE FROM %1 WHERE id IN (SELECT id FROM %1 ORDER BY modified ASC LIMIT 1)").arg(m_Table))) {

                            } else {
                                qWarning().nospace().noquote() << "failed to exec sql\n" << q.lastQuery() << ", error: " << q.lastError();
                                return;
                            }

                            endRemoveRows();
                        }

                        m_IndexCache = -1;

                        beginInsertRows(QModelIndex(), 0, 0);

                        if (q.prepare(insertRowSql)) {
                            auto beg = pairs.cbegin();
                            auto end = pairs.cend();
                            for (auto it = beg; it != end; ++it) {
                                qDebug().nospace().noquote() << "\t" << it.value() << "\n";
                                q.addBindValue(it.value());
                            }

                            q.addBindValue(QDateTime::currentDateTime());

                            if (q.exec()) {
                                ++m_RowCount;
                                qDebug().nospace().noquote() << "new row, count " << m_RowCount;
                            } else {
                                qWarning().nospace().noquote() << "failed to exec insert, error: " << q.lastError();
                            }
                        } else {
                            qWarning().nospace().noquote() << "failed to prepare insert, sql: " << insertRowSql << ", error: " << q.lastError();
                        }
                        endInsertRows();
                    }
                } else {
                    qWarning().nospace().noquote() << "failed to exec insert, error: " << q.lastError();
                }
            } else {
                qWarning().nospace().noquote() << "failed to prepare insert, sql: " << insertRowSql << ", error: " << q.lastError();
            }

    } else {
        qWarning().noquote().nospace() << "add: not ready";
    }
}

//Qt::ItemFlags
//ScRecentlyUsedModel::flags(const QModelIndex &index) const {
//    return Qt::ItemFlags(Qt::DisplayRole | Qt::ItemIsEditable);
//}

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
//        qDebug() << "columns in list model " << QAbstractItemModel::columnCount(QModelIndex());
        if (createTable()) {
            m_Ready = true;
        }
    }

    if (m_Ready) {
        QSqlQuery q(m_Db);
        auto sql = QStringLiteral("SELECT COUNT(*) FROM %1").arg(m_Table);
        if (q.exec(sql)) {
            if (q.next()) {
                m_RowCount = q.value(0).toInt();

                if (m_RowCount > m_Count) {
                    auto n = QString::number(m_RowCount-m_Count);
                    beginResetModel(); // this is here so that views fetch and sort items
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
            qWarning().noquote().nospace() << "failed to fetch row count using sql: " << sql << ", error: " << q.lastError();
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
