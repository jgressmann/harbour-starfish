#pragma once

#include <QObject>


class ScUrlShareItem;
class ScVodPlaylist : public QObject
{
    Q_OBJECT
    Q_PROPERTY(int parts READ parts NOTIFY partsChanged)
    Q_PROPERTY(int startOffset READ startOffset WRITE setStartOffset NOTIFY durationChanged)
    Q_PROPERTY(int duration READ duration NOTIFY durationChanged)
    Q_PROPERTY(bool isValid READ isValid NOTIFY isValidChanged)
public:
    explicit ScVodPlaylist(QObject *parent = nullptr);

    int startOffset() const { return m_StartOffset; }
    void setStartOffset(int value);
    int parts() const { return m_Urls.size(); }
    bool isValid() const;
    Q_INVOKABLE QString url(int index) const;
    Q_INVOKABLE int duration(int index) const;
    Q_INVOKABLE void fromUrl(const QString& url);
    Q_INVOKABLE void fromUrlShareItem(const ScUrlShareItem* item);

signals:
    void partsChanged();
    void durationChanged();
    void startOffsetChanged();
    void isValidChanged();

private:
    QList<QString> m_Urls;
    QList<QString> m_Durations;
    int m_StartOffset;
};
