#include "DistinctModel.h"

DistinctModel::~DistinctModel() {

}

DistinctModel::DistinctModel(QObject* parent)
    : QAbstractProxyModel(parent)
    , m_RoleIndex(-1)
{
    //connect(this, SIGNAL(sourceModelChanged()), this, SLOT(onSourceModelChanged()));

}

QByteArray
DistinctModel::role() const {
    return m_RoleName;
}

void
DistinctModel::setRole(const QByteArray& r) {
    if (r != m_RoleName) {
        m_RoleName = r;
        emit roleChanged(m_RoleName);
        QAbstractItemModel* model = sourceModel();
        if (model) {
            setSourceModel(Q_NULLPTR);
            setSourceModel(model);
        }
    }
}

QModelIndex
DistinctModel::index(int row, int column, const QModelIndex &parent) const {
    if (m_RoleIndex >= 0) {
        if (column == 0 && row >= 0 && row < m_Unique.size()) {
            return createIndex(row, column, Q_NULLPTR);
        }

        return QModelIndex();
    }

    QAbstractItemModel* source = sourceModel();
    if (source) {
        return source->index(row, column, parent);
    }

    return QModelIndex();
}

QModelIndex
DistinctModel::parent(const QModelIndex &child) const {
    if (m_RoleIndex >= 0) {
        return QModelIndex();
    }

    QAbstractItemModel* source = sourceModel();
    if (source) {
        return source->parent(child);
    }

    return QModelIndex();
}
int
DistinctModel::rowCount(const QModelIndex &parent) const {
    if (parent.isValid()) {
        return 0;
    }

    if (m_RoleIndex >= 0) {
        return m_Unique.size();
    }

    QAbstractItemModel* source = sourceModel();
    if (source) {
        return source->rowCount(parent);
    }

    return 0;
}

int
DistinctModel::columnCount(const QModelIndex &parent) const {
    if (parent.isValid()) {
        return 0;
    }

    if (m_RoleIndex >= 0) {
        return 1;
    }

    QAbstractItemModel* source = sourceModel();
    if (source) {
        return source->columnCount(parent);
    }

    return 0;
}

QModelIndex
DistinctModel::mapToSource(const QModelIndex &proxyIndex) const {
    //https://stackoverflow.com/questions/18128722/convert-qsqlquerymodel-data-into-qvectors/18130955#18130955
    if (m_RoleIndex >= 0 ) {
        QAbstractItemModel* source = sourceModel();
        if (source) {
            if (proxyIndex.isValid() && proxyIndex.row() < m_Unique.size() && proxyIndex.column() == 0) {
                return source->index(m_Unique[proxyIndex.row()].second, 0);
            }
        }
    }

    return QModelIndex();
}

QModelIndex
DistinctModel::mapFromSource(const QModelIndex &sourceIndex) const {
    if (m_RoleIndex >= 0 ) {
        QAbstractItemModel* source = sourceModel();
        if (source) {
            QVariant value = source->data(sourceIndex, m_RoleIndex);
            int index = find(value);
            if (index >= 0) {
                return createIndex(index, 0, Q_NULLPTR);
            }
        }
    }

    return QModelIndex();

}

void
DistinctModel::setSourceModel(QAbstractItemModel *newSourceModel) {
    QAbstractItemModel* source = sourceModel();
    if (source) {
        disconnect(source, SIGNAL(dataChanged(const QModelIndex &, const QModelIndex &, const QVector<int> &)), this, SLOT(updateModel()));
        disconnect(source, SIGNAL(rowsInserted(const QModelIndex &, int, int)), this, SLOT(updateModel()));
        disconnect(source, SIGNAL(rowsRemoved(const QModelIndex &, int, int)), this, SLOT(updateModel()));

        if (m_RoleIndex >= 0) {
            beginRemoveColumns(createIndex(0, 0, Q_NULLPTR), 0, 0);
            if (m_Unique.size()) {
                beginRemoveRows(createIndex(0, 0, Q_NULLPTR), m_Unique.size(), m_Unique.size()-1);
                m_Unique.clear();
                endRemoveRows();
            }
            endRemoveColumns();

            m_RoleIndex = -1;
        }
    }

    QAbstractProxyModel::setSourceModel(newSourceModel);

    source = sourceModel();
    if (source) {
        QHash<int, QByteArray> sourceRoleNames(source->roleNames());
        const QHash<int, QByteArray>::const_iterator beg = sourceRoleNames.cbegin();
        const QHash<int, QByteArray>::const_iterator end = sourceRoleNames.cend();
        for (QHash<int, QByteArray>::const_iterator it = beg; it != end; ++it) {
            if (it.value() == m_RoleName) {
                m_RoleIndex = it.key();
                break;
            }
        }

        if (m_RoleIndex != -1) {
            connect(source, SIGNAL(dataChanged(const QModelIndex &, const QModelIndex &, const QVector<int> &)), this, SLOT(updateModel()));
            connect(source, SIGNAL(rowsInserted(const QModelIndex &, int, int)), this, SLOT(updateModel()));
            connect(source, SIGNAL(rowsRemoved(const QModelIndex &, int, int)), this, SLOT(updateModel()));

            beginInsertColumns(QModelIndex(), 0, 0);
            endInsertColumns();

            updateModel();
        }
    }
}

void
DistinctModel::updateModel() {
    QAbstractItemModel* source = sourceModel();
    Q_ASSERT(source);
    Q_ASSERT(m_RoleIndex >= 0);

//    if (m_Unique.size()) {
//        beginRemoveRows(createIndex(0, 0, Q_NULLPTR), m_Unique.size(), m_Unique.size()-1);
//        m_Unique.clear();
//        endRemoveRows();
//    }

    List unique;
    int rows = source->rowCount();
    for (int i = 0; i < rows; ++i) {
        QVariant value = source->data(source->index(i, 0), m_RoleIndex);
        int index = find(unique, value);
        if (index == -1) {
            unique.append(qMakePair(value, i));
        }
    }

    int diff = unique.size() - m_Unique.size();
    bool changed = true;
    if (diff > 0) {
        changed = m_Unique.size() > 0;
        beginInsertRows(QModelIndex(), m_Unique.size(), unique.size()-1);
        m_Unique.swap(unique);
        endInsertRows();
    } else if (diff == 0) {
        changed = m_Unique.size() > 0;
        m_Unique.swap(unique);
    } else {
        beginRemoveRows(QModelIndex(), unique.size(), m_Unique.size()-1);
        m_Unique.swap(unique);
        endRemoveRows();
    }

    if (changed) {
        emit dataChanged(createIndex(0, 0, Q_NULLPTR), createIndex(m_Unique.size()-1, 0, Q_NULLPTR));
    }
}

int
DistinctModel::find(const List& li, const QVariant& value) {
    for (int i = 0; i < li.size(); ++i) {
        if (value == li[i].first) {
            return i;
        }
    }

    return -1;
}

QHash<int, QByteArray>
DistinctModel::roleNames() const {
    QHash<int, QByteArray> result;
//    QAbstractItemModel* source = sourceModel();
//    if (source) {
//        QHash<int, QByteArray> sourceRoleNames(source->roleNames());
//        QHash<int, QByteArray>::const_iterator it = sourceRoleNames.find(m_RoleIndex);
//        if (it != sourceRoleNames.end()) {
//            result.insert(m_RoleIndex, it.value());
//        }
//    }

    if (-1 != m_RoleIndex) {
        result.insert(m_RoleIndex, m_RoleName);
    }
    return result;
}
