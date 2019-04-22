#include "ScVodPlaylist.h"
#include "ScUrlShareItem.h"
#include "ScVodFileItem.h"

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

void
ScVodPlaylist::fromUrl(const QString& url)
{
    m_Urls.clear();
    m_Urls.append(url);
    m_Duration = -1;
    setStartOffset(0);
    emit durationChanged();
    emit partsChanged();
    emit isValidChanged();
}

void
ScVodPlaylist::fromUrlShareItem(const ScUrlShareItem* item)
{
    if (!item) {
        qCritical() << "item is null";
        return;
    }

    m_Urls.clear();
    m_Urls.reserve(item->files());
    m_Durations.clear();
    m_Durations.reserve(item->files());
    for (auto i = 0; i < m_Urls.size(); ++i) {
        m_Urls.append(item->file(i)->vodFilePath());
        m_Durations.append(-1);
    }

    setStartOffset(0);

    emit durationChanged();
    emit partsChanged();
    emit isValidChanged();
}

QString
ScVodPlaylist::url(int index) const
{
    if (index >= 0 && index < m_Urls.size()) {
        return m_Urls[index];
    }

    qCritical() << "index out of range" << index;
    return QString();
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

bool
ScVodPlaylist::isValid() const
{
    return m_Urls.size() > 0;
}
