#pragma once

#include <QObject>
#include <QVector>

//class ScUrlShareItem;
class ScVodPlaylist : public QObject
{
    Q_OBJECT
    Q_PROPERTY(int parts READ parts WRITE setParts NOTIFY partsChanged)
    Q_PROPERTY(int startOffset READ startOffset WRITE setStartOffset NOTIFY startOffsetChanged)
    Q_PROPERTY(bool isValid READ isValid NOTIFY isValidChanged)
public:
    explicit ScVodPlaylist(QObject *parent = nullptr);

    int startOffset() const { return m_StartOffset; }
    void setStartOffset(int value);
    int parts() const { return m_Urls.size(); }
    void setParts(int value);
    bool isValid() const;
    Q_INVOKABLE QString url(int index) const;
    Q_INVOKABLE void setUrl(int index, const QString& url);
    Q_INVOKABLE int duration(int index) const;
    Q_INVOKABLE void setDuration(int index, int value);
//    Q_INVOKABLE void fromUrl(const QString& value);
//    Q_INVOKABLE void fromUrlShareItem(const ScUrlShareItem* item);

signals:
    void partsChanged();
    void startOffsetChanged();
    void isValidChanged();

private:
    QVector<QString> m_Urls;
    QVector<int> m_Durations;
    int m_StartOffset;
};

QDebug operator<<(QDebug debug, const ScVodPlaylist& value);
