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
        emit selectChanged(m_Select);
        m_Dirty = true;
        update();
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
            disconnect(m_VodModel, SIGNAL(vodsChanged()), this, SLOT(update()));
        }

        m_VodModel = newValue;
        emit vodModelChanged(m_VodModel);

        if (m_VodModel) {
            connect(m_VodModel, SIGNAL(vodsChanged()), this, SLOT(update()));
            update();
        }
    }
}
void
SqlVodModel::update() {
    if (m_Dirty) {
        m_Dirty = !tryConfigureModel();
    } else {
        //beginResetModel();
        setQuery(m_Select, *m_VodModel->database()); // without this line there is no update in rowCount
        //endResetModel();

        return;
        auto status = m_VodModel->status();
        if (status == VodModel::Status_VodFetchingComplete) {
//            tryConfigureModel();
            beginResetModel();
            setQuery(m_Select, *m_VodModel->database()); // without this line there is no update in rowCount
            endResetModel();
//            qDebug() << m_Select << ":" << rowCount() << "rows" << columnCount() << "cols";

//            QSqlQuery q(*m_VodModel->database());
//            if (!q.exec("select count(*) from vods") || !q.next()) {
//                return;
//            }

//            qDebug() << q.value(0).toInt() << "rows";

//            if (!q.exec("select distinct game from vods")) {
//                return;
//            }

//            qDebug() << "games:";
//            while (q.next()) {
//                qDebug() << q.value(0).toInt();

//            }

        }
    }
}

bool
SqlVodModel::tryConfigureModel() {
    if (m_VodModel && m_VodModel->database() &&
       !m_Select.isEmpty() &&
       !m_Columns.empty()) {

        setQuery(m_Select, *m_VodModel->database());
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
    setQuery(m_Select, *m_VodModel->database());
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
