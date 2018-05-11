/* The MIT License (MIT)
 *
 * Copyright (c) 2018 Jean Gressmann <jean@0x42.de>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#pragma once

#include <QSqlQueryModel>
#include <QList>

class ScVodDataManager;
class ScSqlVodModel : public QSqlQueryModel {
    Q_OBJECT
    Q_PROPERTY(QString select READ select WRITE setSelect NOTIFY selectChanged)
    Q_PROPERTY(ScVodDataManager* dataManager READ dataManager WRITE setDataManager NOTIFY dataManagerChanged)
    Q_PROPERTY(QStringList columns READ columns WRITE setColumns NOTIFY columnsChanged)
public:
    ~ScSqlVodModel();
    explicit ScSqlVodModel(QObject* parent = Q_NULLPTR);

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


