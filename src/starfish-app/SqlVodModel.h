#pragma once

#include <QSqlQueryModel>
#include <QList>

class ScVodDataManager;
class SqlVodModel : public QSqlQueryModel {
    Q_OBJECT
    Q_PROPERTY(QString select READ select WRITE setSelect NOTIFY selectChanged)
    Q_PROPERTY(ScVodDataManager* dataManager READ dataManager WRITE setDataManager NOTIFY dataManagerChanged)
    Q_PROPERTY(QStringList columns READ columns WRITE setColumns NOTIFY columnsChanged)
public:
    ~SqlVodModel();
    explicit SqlVodModel(QObject* parent = Q_NULLPTR);

public:
    QString select() const;
    void setSelect(QString newValue);
    ScVodDataManager* dataManager() const;
    void setDataManager(ScVodDataManager* newValue);
    QStringList columns() const;
    void setColumns(const QStringList& newValue);
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const Q_DECL_OVERRIDE;
    QHash<int,QByteArray> roleNames() const Q_DECL_OVERRIDE;

signals:
    void selectChanged();
    void dataManagerChanged();
    void columnsChanged();

private:
    void update();
    bool tryConfigureModel();
    void refresh();

private:
    QHash<int, QByteArray> m_RoleNames;
    QString m_Select;
    ScVodDataManager* m_DataManager;
    QStringList m_Columns;
    bool m_Dirty;
};


