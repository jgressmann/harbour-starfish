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


#include "ScDatabaseStoreQueue.h"
#include "ScDatabase.h"

#include <QSqlError>
#include <QDebug>

namespace {
static const QString s_Begin = QStringLiteral("BEGIN");
static const QString s_Commit = QStringLiteral("COMMIT");
static const QString s_Rollback = QStringLiteral("ROLLBACK");
} // anon


ScDatabaseStoreQueue::~ScDatabaseStoreQueue()
{
    m_Query = QSqlQuery();
    m_Database = QSqlDatabase();
    QSqlDatabase::removeDatabase(QStringLiteral("ScDatabaseStoreQueue"));
}

ScDatabaseStoreQueue::ScDatabaseStoreQueue(const QString& databasePath, QObject* parent)
    : QObject(parent)
    , m_Database(QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), QStringLiteral("ScDatabaseStoreQueue")))
{
    m_LastTransactionId = InvalidToken;

    m_Database.setDatabaseName(databasePath);
    if (!m_Database.open()) {
        qCritical("Could not open database '%s'. Error: %s\n", qPrintable(databasePath), qPrintable(m_Database.lastError().text()));
        return;
    }

    m_Query = QSqlQuery(m_Database);

    if (!scSetupWriteableConnection(m_Database)) {
        return;
    }

//    if (!m_Query.exec(QStringLiteral("PRAGMA foreign_keys=ON"))) {
//        qCritical("Failed to enable foreign_key on database connection: %s\n", qPrintable(m_Database.lastError().text()));
//        return;
//    }

//    if (!m_Query.exec(QStringLiteral("PRAGMA foreign_keys"))) {
//        qCritical("Could not query foreign_key setting: %s\n", qPrintable(m_Database.lastError().text()));
//        return;
//    }

//    if (!m_Query.next()) {
//        qCritical("Could not fetch result for foreign_key query: %s\n", qPrintable(m_Database.lastError().text()));
//        return;
//    }

//    qDebug() << "foreign_keys" << m_Query.value(0).toInt();
}

int ScDatabaseStoreQueue::getToken()
{
    auto token = m_TokenGenerator++;
    if (token == InvalidToken) {
        return getToken();
    }
    return token;
}

int ScDatabaseStoreQueue::newTransactionId()
{
    return getToken();
}

void ScDatabaseStoreQueue::process(int transactionId, const QString& sql, const QVariantList& args)
{
    processStatement(transactionId, sql, args);

    if (InvalidToken == m_LastTransactionId) { // no active transaction?

        // Multiple interleaved transactions are possible.
        // Hence run the interleaved ones that were picked up during processing
        // of the active transaction now that the active one has completed.

        auto cbeg = m_CompleteTransactions.cbegin();
        auto cend = m_CompleteTransactions.cend();
        for (auto cit = cbeg; cit != cend; ++cit) {
            auto beg = cit->cbegin();
            auto end = cit->cend();

            for (auto it = beg; it != end; ++it) {
                processStatement(it->token, it->sql, it->args);
            }
        }

        m_CompleteTransactions.clear();
    }
}

void ScDatabaseStoreQueue::processStatement(int transactionId, const QString& sql, const QVariantList& args)
{
    if (InvalidToken == m_LastTransactionId) { // no active transaction
        auto it = m_IncompleteTransactions.find(transactionId);
        if (it == m_IncompleteTransactions.end()) { // set as active transaction
            m_LastTransactionId = transactionId;

            if (m_Query.exec(s_Begin)) {
#if !defined(QT_NO_DEBUG) && defined(STARFISH_DEBUG_DB)
                qDebug("%s\n", qPrintable(s_Begin));
#endif
            } else {
                emit completed(m_LastTransactionId, getInsertIdOrAffectedRows(), m_Query.lastError().number(), m_Query.lastError().databaseText());
                abort();
            }
        } else {
            it.value().append({ sql, args, transactionId });

            if (sql.isEmpty()) { // end of transaction
                m_CompleteTransactions.append(it.value());
                m_IncompleteTransactions.erase(it);
            }
        }
    }

    if (m_LastTransactionId == transactionId) {
        if (sql.isEmpty()) { // end transaction guard
            if (m_Query.exec(s_Commit)) {
                emit completed(m_LastTransactionId, getInsertIdOrAffectedRows(), 0, QString());
#if !defined(QT_NO_DEBUG) && defined(STARFISH_DEBUG_DB)
                qDebug("%s\n", qPrintable(s_Commit));
#endif
            } else {
                emit completed(m_LastTransactionId, getInsertIdOrAffectedRows(), m_Query.lastError().number(), m_Query.lastError().databaseText());
            }

            m_Query.finish();

            m_LastTransactionId = InvalidToken;
        } else {
            if (m_Query.prepare(sql)) {
                auto beg = args.cbegin();
                auto end = args.cend();
                for (auto it = beg; it != end; ++it) {
                    m_Query.addBindValue(*it);
                }
                if (m_Query.exec()) {
#if !defined(QT_NO_DEBUG) && defined(STARFISH_DEBUG_DB)
                    qDebug("%s\n", qPrintable(m_Query.executedQuery()));
                    for (auto i = 0; i < args.size(); ++i) {
                        qDebug("%d. %s\n", i, qPrintable(args[i].toString()));
                    }
#endif
                    //emit completed(t.token, getInsertIdOrAffectedRows(), 0, QString());
                } else {
                    qDebug() << "failed" << sql;
                    emit completed(transactionId, getInsertIdOrAffectedRows(), m_Query.lastError().number(), m_Query.lastError().databaseText());
                    abort();
                }
            } else {
                emit completed(transactionId, getInsertIdOrAffectedRows(), m_Query.lastError().number(), m_Query.lastError().databaseText());
                abort();
            }
        }
    } else { // transaction interleaving
        auto it = m_IncompleteTransactions.find(transactionId);
        if (it == m_IncompleteTransactions.end()) {
            it = m_IncompleteTransactions.insert(transactionId, QList<SqlData>());
        }

        it.value().append({ sql, args, transactionId });

        if (sql.isEmpty()) { // end of transaction
            m_CompleteTransactions.append(QList<SqlData>());
            m_CompleteTransactions.last().swap(it.value());
            m_IncompleteTransactions.erase(it);
        }
    }
}

qint64 ScDatabaseStoreQueue::getInsertIdOrAffectedRows() const
{
    auto insertId = m_Query.lastInsertId();
    if (insertId.isValid()) {
        return qvariant_cast<qint64>(insertId);
    }

    return m_Query.numRowsAffected();
}

void ScDatabaseStoreQueue::abort()
{
    m_Query.exec(s_Rollback);
    m_LastTransactionId = InvalidToken;
}
