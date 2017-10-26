#pragma once

#include <QAbstractProxyModel>
#include <QHash>

class DistinctModel : public QAbstractProxyModel {
    Q_OBJECT
    Q_PROPERTY(QByteArray role READ role WRITE setRole NOTIFY roleChanged)
public:
    ~DistinctModel();
    DistinctModel(QObject* parent = Q_NULLPTR);

public:
    QByteArray role() const;
    void setRole(const QByteArray& r);
    QModelIndex index(int row, int column, const QModelIndex &parent = QModelIndex()) const;
    QModelIndex parent(const QModelIndex &child) const;
    int rowCount(const QModelIndex &parent = QModelIndex()) const;
    int columnCount(const QModelIndex &parent = QModelIndex()) const;
    QModelIndex mapToSource(const QModelIndex &proxyIndex) const;
    QModelIndex mapFromSource(const QModelIndex &sourceIndex) const;
    QHash<int, QByteArray> roleNames() const;
    void setSourceModel(QAbstractItemModel *sourceModel);


protected:

signals:
    void roleChanged(const QByteArray& newRole);

private slots:
    void updateModel();

private:
    typedef QList<QPair<QVariant, int> > List;
    static int find(const List& li, const QVariant& value);
    inline int find(const QVariant& value) const { return find(m_Unique, value); }

private:
    QByteArray m_RoleName;
    List m_Unique;
    int m_RoleIndex;
};
