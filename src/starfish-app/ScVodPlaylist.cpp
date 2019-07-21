/* The MIT License (MIT)
 *
 * Copyright (c) 2019 Jean Gressmann <jean@0x42.de>
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

#include "ScVodPlaylist.h"

#include <QDebug>

ScVodPlaylist::ScVodPlaylist(QObject *parent)
    : QObject(parent)
{
    m_StartOffset = 0;
    m_EndOffset = -1;
    m_PlaybackOffset = 0;
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

void
ScVodPlaylist::setEndOffset(int value)
{
    if (value != m_EndOffset) {
        m_EndOffset = value;
        emit endOffsetChanged();
    }
}

void
ScVodPlaylist::setPlaybackOffset(int value)
{
    if (value != m_PlaybackOffset) {
        m_PlaybackOffset = value;
        emit playbackOffsetChanged();
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
    m_EndOffset = other.m_EndOffset;
    m_PlaybackOffset = other.m_PlaybackOffset;

    emit startOffsetChanged();
    emit endOffsetChanged();
    emit playbackOffsetChanged();
    emit isValidChanged();
}

QDebug operator<<(QDebug debug, const ScVodPlaylist& value)
{
    QDebugStateSaver saver(debug);
    debug.nospace() << "ScVodPlaylist("
                    << "startOffset=" << value.startOffset()
                    << "endOffset=" << value.endOffset()
                    << "playbackOffset=" << value.playbackOffset()
                    << ", parts=" << value.parts()
                    << ", valid=" << value.isValid()
                    << ")";
    return debug;
}
