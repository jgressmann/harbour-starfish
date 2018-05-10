#include "SqlVodModel.h"
#include "ScVodDataManager.h"
#include <QSqlQuery>
#include <QSqlError>
#include <QDebug>


// https://wiki.qt.io/How_to_Use_a_QSqlQueryModel_in_QML

SqlVodModel::~SqlVodModel() {

}

SqlVodModel::SqlVodModel(QObject* parent)
    : QSqlQueryModel(parent)
{
    m_DataManager = Q_NULLPTR;
    m_Dirty = false;
}

QVariant
SqlVodModel::data(const QModelIndex &modelIndex, int role) const {
    if (role < Qt::UserRole) {
        return QSqlQueryModel::data(modelIndex, role);
    }

    int column = role - Qt::UserRole - 1;
    QModelIndex remappedIndex = index(modelIndex.row(), column);
    return QSqlQueryModel::data(remappedIndex, Qt::DisplayRole);
}

QHash<int,QByteArray>
SqlVodModel::roleNames() const {
    return m_RoleNames;
}

QString
SqlVodModel::select() const {
    return m_Select;
}

void
SqlVodModel::setSelect(QString newValue) {
    if (m_Select != newValue) {
        m_Select = newValue;
        emit selectChanged();
        m_Dirty = true;
        update();
    }
}

ScVodDataManager*
SqlVodModel::dataManager() const {
    return m_DataManager;
}

void
SqlVodModel::setDataManager(ScVodDataManager* newValue) {
    if (newValue != m_DataManager) {
        m_Dirty = true;

        if (m_DataManager) {
            disconnect(m_DataManager, &ScVodDataManager::vodsChanged,
                       this,  &SqlVodModel::update);
        }

        m_DataManager = newValue;
        emit dataManagerChanged();

        if (m_DataManager) {
            connect(m_DataManager, &ScVodDataManager::vodsChanged,
                       this,  &SqlVodModel::update);
            update();
        }
    }
}



void
SqlVodModel::update() {
    if (m_Dirty) {
        m_Dirty = !tryConfigureModel();
    } else {
        setQuery(m_Select, m_DataManager->database());
    }
}

bool
SqlVodModel::tryConfigureModel() {
    if (m_DataManager &&
       !m_Select.isEmpty() &&
       !m_Columns.empty()) {

        setQuery(m_Select, m_DataManager->database());
        beginResetModel();
        for (int i = 0; i < m_Columns.count(); ++i) {
            setHeaderData(0, Qt::Horizontal, m_Columns[i]);
        }
        endResetModel();
//        refresh();

        return true;
    }

    return false;
}

void
SqlVodModel::refresh() {
    beginResetModel();
    setQuery(m_Select, m_DataManager->database());
    for (int i = 0; i < m_Columns.count(); ++i) {
        setHeaderData(0, Qt::Horizontal, m_Columns[i]);
    }
    endResetModel();
}

QStringList
SqlVodModel::columns() const {
    return m_Columns;
}

void
SqlVodModel::setColumns(const QStringList& newValue) {
    m_Dirty = true;
    m_Columns = newValue;
    m_RoleNames.clear();
    for (int i = 0; i < m_Columns.count(); ++i) {
        m_RoleNames[Qt::UserRole + i + 1] = m_Columns[i].toLocal8Bit();
    }
    emit columnsChanged();
    update();
}
