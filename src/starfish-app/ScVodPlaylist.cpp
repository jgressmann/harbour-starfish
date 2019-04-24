#include "ScVodPlaylist.h"
//#include "ScUrlShareItem.h"
//#include "ScVodFileItem.h"

#include <QDebug>

ScVodPlaylist::ScVodPlaylist(QObject *parent)
    : QObject(parent)
{
    m_StartOffset = 0;
}

void
ScVodPlaylist::setStartOffset(int value)
{
    if (value != m_StartOffset) {
        m_StartOffset = value;
        emit startOffsetChanged();
    }
}

//void
//ScVodPlaylist::fromUrl(const QString& url)
//{
//    m_Urls.clear();
//    m_Urls.append(url);
//    m_Durations.clear();
//    m_Durations.append(-1);
//    setStartOffset(0);
//    emit partsChanged();
//    emit isValidChanged();
//}

//void
//ScVodPlaylist::fromUrlShareItem(const ScUrlShareItem* item)
//{
//    if (!item) {
//        qCritical() << "item is null";
//        return;
//    }

//    auto playlist = item->metaData();
//    if (!playlist.isValid()) {
//        qWarning() << "url share item playlist not valid";
//        return;
//    }

//    const auto& vods = playlist._vods();

//    m_Urls.clear();
//    m_Urls.reserve(item->files());
//    m_Durations.clear();
//    m_Durations.reserve(item->files());
//    for (auto i = 0; i < m_Urls.size(); ++i) {
//        m_Urls.append(item->file(i)->vodFilePath());
//        m_Durations.append(vods[i].duration());
//    }

//    setStartOffset(0);

//    emit partsChanged();
//    emit isValidChanged();
//}

QString
ScVodPlaylist::url(int index) const
{
    if (index >= 0 && index < m_Urls.size()) {
        return m_Urls[index];
    }

    qCritical() << "index out of range" << index;
    return QString();
}

void
ScVodPlaylist::setUrl(int index, const QString& value)
{
    if (index >= 0 && index < m_Urls.size()) {
        m_Urls[index] = value;
    } else {
        qCritical() << "index out of range" << index;
    }
}

int
ScVodPlaylist::duration(int index) const
{
    if (index >= 0 && index < m_Urls.size()) {
        return m_Durations[index];
    }

    qCritical() << "index out of range" << index;
    return 0;
}

void
ScVodPlaylist::setDuration(int index, int value)
{
    if (index >= 0 && index < m_Urls.size()) {
        m_Durations[index] = value;
    } else {
        qCritical() << "index out of range" << index;
    }
}

void
ScVodPlaylist::setParts(int value)
{
    if (value >= 0) {
        if (value != m_Durations.size()) {
            m_Durations.resize(value);
            m_Urls.resize(value);
            emit partsChanged();
            emit isValidChanged();
        }
    } else {
        qWarning() << "value must not be negative:" << value;
    }
}

bool
ScVodPlaylist::isValid() const
{
    return m_Urls.size() > 0;
}

QDebug operator<<(QDebug debug, const ScVodPlaylist& value)
{
    QDebugStateSaver saver(debug);
    debug.nospace() << "ScVodPlaylist("
                    << "startOffset=" << value.startOffset()
                    << ", parts=" << value.parts()
                    << ", valid=" << value.isValid()
                    << ")";
    return debug;
}
