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
#include "ScUrlShareItem.h"

class ScVodFileItem : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString vodFilePath READ vodFilePath NOTIFY vodFilePathChanged)
    Q_PROPERTY(quint64 vodFileSize READ vodFileSize NOTIFY vodFileSizeChanged)
    Q_PROPERTY(qreal downloadProgress READ downloadProgress NOTIFY downloadProgressChanged)
    Q_PROPERTY(int duration READ duration NOTIFY durationChanged)
public:
    explicit ScVodFileItem(ScUrlShareItem* parent);

public:
    QString vodFilePath() const { return m_FilePath; }
    qreal downloadProgress() const { return m_Progress; }
    qint64 vodFileSize() const { return m_FileSize; }
    int duration() const { return m_Duration; }
    void onVodAvailable(const ScVodFileFetchProgress& progress);

signals:
    void vodFilePathChanged();
    void vodFileSizeChanged();
    void downloadProgressChanged();
    void durationChanged();

private:
    void setVodFilePath(const QString& value);
    void setDownloadProgress(float value);
    void setFileSize(qint64 value);
    void setDuration(int value);

private:
    QString m_FilePath;
    qint64 m_FileSize;
    qreal m_Progress;
    int m_Duration;
};


