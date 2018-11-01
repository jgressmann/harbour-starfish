#pragma once

#include "ScIcons.h"
#include "ScClassifier.h"
#include <QSqlDatabase>


class ScVodDataManagerState
{
public:
//    QSqlDatabase m_Database;
    ScIcons m_Icons;
    ScClassifier m_Classifier;
    QString m_ThumbnailDir;
    QString m_MetaDataDir;
    QString m_VodDir;
    QString m_IconDir;
    QString DatabaseFilePath;
};
