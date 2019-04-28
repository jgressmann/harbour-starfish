#include "ScVodPlaylist.h"

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
        emit isValidChanged();
    }
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

void
ScVodPlaylist::setUrl(int index, const QString& value)
{
    if (index >= 0 && index < m_Urls.size()) {
        m_Urls[index] = value;
        emit isValidChanged();
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
        emit isValidChanged();
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
    if (m_StartOffset < 0) {
        return false;
    }

    if (m_Urls.isEmpty()) {
        return false;
    }

    for (const auto& url : m_Urls) {
        if (url.isEmpty()) {
            return false;
        }
    }

    return true;
}

void
ScVodPlaylist::copyFrom(const QVariant& value)
{
    const auto h = qvariant_cast<ScVodPlaylistHandle>(value);
    if (h) {
        copyFrom(*h);
    } else {
        qWarning() << "non playlist arg";
    }
}

void
ScVodPlaylist::copyFrom(const ScVodPlaylist& other)
{
    m_Urls = other.m_Urls;
    m_Durations = other.m_Durations;
    m_StartOffset = other.m_StartOffset;
    emit startOffsetChanged();
    emit isValidChanged();
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
