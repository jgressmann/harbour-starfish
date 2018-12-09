#pragma once

#include <QString>
#include <QScopedPointer>
class ScDatabaseStoreQueue;
class ScVodDataManagerState
{
public:
    QString m_ThumbnailDir;
    QString m_MetaDataDir;
    QString m_VodDir;
    QString m_IconDir;
    QString DatabaseFilePath;
    QScopedPointer<ScDatabaseStoreQueue> DatabaseStoreQueue;
};
