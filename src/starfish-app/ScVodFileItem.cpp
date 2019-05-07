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

#include "ScVodFileItem.h"
#include <QDebug>

ScVodFileItem::ScVodFileItem(ScUrlShareItem* parent)
    : QObject(parent)
{
    m_FileSize = 0;
    m_Progress = 0;
    m_Duration = 0;
}

void
ScVodFileItem::setVodFilePath(const QString& value)
{
    if (value != m_FilePath) {
        m_FilePath = value;
        emit vodFilePathChanged();
    }
}

void
ScVodFileItem::setDownloadProgress(float value)
{
    if (value == value && value != m_Progress) {
        m_Progress = value;
        emit downloadProgressChanged();
    }
}

void
ScVodFileItem::setFileSize(qint64 value)
{
    if (value != m_FileSize) {
        m_FileSize = value;
        emit vodFileSizeChanged();
    }
}

void
ScVodFileItem::setDuration(int value)
{
    if (value != m_Duration) {
        m_Duration = value;
        emit durationChanged();
    }
}

void
ScVodFileItem::onVodAvailable(const ScVodFileFetchProgress& progress)
{
    setVodFilePath(progress.filePath);
    setFileSize(progress.fileSize);
    setDuration(progress.duration);
    if (progress.progress >= 0 && progress.progress <= 1) {
        setDownloadProgress(progress.progress);
    } else {
        qWarning() << "out of range download progress" << progress.progress;
    }
}
