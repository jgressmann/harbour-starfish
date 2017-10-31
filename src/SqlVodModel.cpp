#include "SqlVodModel.h"
#include "Sc2LinksDotCom.h"
#include <QSqlQuery>
#include <QSqlError>
#include <QDebug>


// https://wiki.qt.io/How_to_Use_a_QSqlQueryModel_in_QML

SqlVodModel::~SqlVodModel() {

}

SqlVodModel::SqlVodModel(QObject* parent)
    : QSqlQueryModel(parent)
{
    m_VodModel = Q_NULLPTR;
    m_Dirty = false;
}

//int
//SqlVodModel::rowCount(const QModelIndex &parent) const {
//    if (m_Dirty) {
//        SqlVodModel* self = const_cast<SqlVodModel*>(this);
//        self->m_Dirty = !SqlVodModel*tryConfigureModel();
//    }

//    return QSqlQueryModel::rowCount(parent);
//}

//void
//SqlVodModel::queryChange() {
////    if (m_Dirty) {
////        SqlVodModel* self = const_cast<SqlVodModel*>(this);
////        self->m_Dirty = !self->tryConfigureModel();
////    }

//    QSqlQueryModel::queryChange();
//}

QVariant
SqlVodModel::data(const QModelIndex &modelIndex, int role) const {
    if (role < Qt::UserRole) {
        return QSqlQueryModel::data(modelIndex, role);
    }

    int column = role - Qt::UserRole - 1;
    QModelIndex remappedIndex = index(modelIndex.row(), column);
    return QSqlQueryModel::data(remappedIndex, Qt::DisplayRole);
}

QVariant
SqlVodModel::at(int row, int column) const {
    return QSqlQueryModel::data(index(row, column), Qt::DisplayRole);
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
        emit selectChanged(m_Select);
        m_Dirty = true;
        statusChanged();
    }
}

VodModel*
SqlVodModel::vodModel() const {
    return m_VodModel;
}

void
SqlVodModel::setVodModel(VodModel* newValue) {
    if (newValue != m_VodModel) {
        m_Dirty = true;

        if (m_VodModel) {
            disconnect(m_VodModel, SIGNAL(statusChanged()), this, SLOT(statusChanged()));
        }

        m_VodModel = newValue;
        emit vodModelChanged(m_VodModel);

        if (m_VodModel) {
            connect(m_VodModel, SIGNAL(statusChanged()), this, SLOT(statusChanged()));
            statusChanged();
        }
    }
}

void
SqlVodModel::statusChanged() {
    if (m_Dirty) {
        m_Dirty = !tryConfigureModel();
    } else {
        auto status = m_VodModel->status();
        if (status == VodModel::Status_VodFetchingComplete) {
            beginResetModel();
            endResetModel();
        }
    }
}

bool
SqlVodModel::tryConfigureModel() {
    if (m_VodModel && m_VodModel->database() &&
       !m_Select.isEmpty() &&
       !m_Columns.empty()) {

        setQuery(m_Select, *m_VodModel->database());
//        QSqlError error = lastError();
//        if (error.isValid()) {
//            qWarning() << error;
//            return false;
//        }

        beginResetModel();
        for (int i = 0; i < m_Columns.count(); ++i) {
            setHeaderData(0, Qt::Horizontal, m_Columns[i]);
        }
        endResetModel();

        return true;
    }

    return false;
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
    statusChanged();
}
