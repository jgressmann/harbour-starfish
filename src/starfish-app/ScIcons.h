#pragma once

#include <QString>
#include <QList>
#include <QRegExp>


class ScIcons {
public:
    bool load(QString path);
    bool getIconForEvent(const QString& event, QString* url) const;
    int size() const;
    QString url(int index) const;
    QString extension(int index) const;

private:
    struct ScIconData {
        QRegExp regex;
        QString url;
        QString extension;
    };
private:
    QList<ScIconData> m_IconData;
};
