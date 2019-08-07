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

#pragma once

#include <QObject>
#include <QVector>
#include <QVariant>

class ScVodPlaylist : public QObject
{
    Q_OBJECT
    Q_PROPERTY(int parts READ parts WRITE setParts NOTIFY partsChanged)
    Q_PROPERTY(int startOffset READ startOffset WRITE setStartOffset NOTIFY startOffsetChanged)
    Q_PROPERTY(int endOffset READ endOffset WRITE setEndOffset NOTIFY endOffsetChanged)
    Q_PROPERTY(int playbackOffset READ playbackOffset WRITE setPlaybackOffset NOTIFY playbackOffsetChanged)
    Q_PROPERTY(bool isValid READ isValid NOTIFY isValidChanged)
    Q_PROPERTY(QVariant mediaKey READ mediaKey WRITE setMediaKey NOTIFY mediaKeyChanged)
public:
    explicit ScVodPlaylist(QObject *parent = nullptr);

    int startOffset() const { return m_StartOffset; }
    void setStartOffset(int value);
    int endOffset() const { return m_EndOffset; }
    void setEndOffset(int value);
    int playbackOffset() const { return m_PlaybackOffset; }
    void setPlaybackOffset(int value);
    int parts() const { return m_Urls.size(); }
    void setParts(int value);
    bool isValid() const;
    QVariant mediaKey() const { return m_MediaKey; }
    void setMediaKey(const QVariant& value);
    Q_INVOKABLE QString url(int index) const;
    Q_INVOKABLE void setUrl(int index, const QString& url);
    Q_INVOKABLE int duration(int index) const;
    Q_INVOKABLE void setDuration(int index, int value);
    Q_INVOKABLE void copyFrom(const QVariant& other);
    Q_INVOKABLE void invalidate();


signals:
    void partsChanged();
    void startOffsetChanged();
    void endOffsetChanged();
    void playbackOffsetChanged();
    void mediaKeyChanged();
    void isValidChanged();

private:
    void copyFrom(const ScVodPlaylist& other);

private:
    QVector<QString> m_Urls;
    QVector<int> m_Durations;
    QVariant m_MediaKey;
    int m_StartOffset, m_EndOffset, m_PlaybackOffset;
};

using ScVodPlaylistHandle = ScVodPlaylist*;

Q_DECLARE_METATYPE(ScVodPlaylistHandle)

QDebug operator<<(QDebug debug, const ScVodPlaylist& value);
