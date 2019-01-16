#include "ScDatabase.h"

#include <QSqlDatabase>
#include <QSqlQuery>
#include <QDebug>
#include <QSqlError>

bool scSetupWriteableConnection(const QSqlDatabase& db)
{
    QSqlQuery q(db);
    if (!q.exec("PRAGMA foreign_keys=ON")) {
        qCritical() << "failed to enable foreign keys" << q.lastError();
        return false;
    }

    // https://codificar.com.br/blog/sqlite-optimization-faq/
    if (!q.exec("PRAGMA count_changes=OFF")) {
        qCritical() << "failed to disable change counting" << q.lastError();
        return false;
    }

    if (!q.exec("PRAGMA temp_store=MEMORY")) {
        qCritical() << "failed to set temp store to memory" << q.lastError();
        return false;
    }

    return true;
}
