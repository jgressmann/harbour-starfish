#pragma once

#include <QSqlQueryModel>
#include <QQmlListProperty>
#include <QList>

class VodModel;
class SqlVodModel : public QSqlQueryModel {
    Q_OBJECT
    Q_PROPERTY(QString select READ select WRITE setSelect NOTIFY selectChanged)
    Q_PROPERTY(VodModel* vodModel READ vodModel WRITE setVodModel NOTIFY vodModelChanged)
    Q_PROPERTY(QStringList columns READ columns WRITE setColumns NOTIFY columnsChanged)
public:
    ~SqlVodModel();
    explicit SqlVodModel(QObject* parent = Q_NULLPTR);

public:
    QString select() const;
    void setSelect(QString newValue);
    VodModel* vodModel() const;
    void setVodModel(VodModel* newValue);
    QStringList columns() const;
    void setColumns(const QStringList& newValue);
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const Q_DECL_OVERRIDE;
    QHash<int,QByteArray> roleNames() const Q_DECL_OVERRIDE;
    Q_INVOKABLE QVariant at(int row, int role) const;
signals:
    void selectChanged(QString newvalue);
    void vodModelChanged(VodModel* newvalue);
    void columnsChanged();

private slots:
    void statusChanged();

private:
    bool tryConfigureModel();

private:
    QHash<int, QByteArray> m_RoleNames;
    QString m_Select;
    VodModel* m_VodModel;
    QStringList m_Columns;
    bool m_Dirty;
};


