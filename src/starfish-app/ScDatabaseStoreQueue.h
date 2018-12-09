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

#include <QSqlDatabase>
#include <QSqlQuery>
#include <QVariantList>
#include <QAtomicInt>
#include <QSemaphore>

class ScDatabaseStoreQueue : public QObject
{
    Q_OBJECT
public:
    static constexpr int InvalidToken = -1;
    ~ScDatabaseStoreQueue();
    ScDatabaseStoreQueue(const QString& databasePath, QObject* parent = nullptr);

    int newTransactionId();
    void requestSlot();

signals:
    void completed(int token, qint64 insertIdOrNumRowsAffected, int error, QString errorDescription);

public slots:
    void process(int transactionId, const QString& sql, const QVariantList& args);

private:
    struct SqlData
    {
        QString sql;
        QVariantList args;
        int token;
    };

    qint64 getInsertIdOrAffectedRows() const;
    void processStatement(int transactionId, const QString& sql, const QVariantList& args);
    void abort();
    int getToken();

private:
    QSqlDatabase m_Database;
    QSemaphore m_Semaphore;
    QSqlQuery m_Query;
    QAtomicInt m_TokenGenerator;
    QHash<int, QList<SqlData> > m_IncompleteTransactions;
    QList< QList<SqlData> > m_CompleteTransactions;
    int m_LastTransactionId;
};
