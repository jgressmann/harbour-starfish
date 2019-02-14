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
#include <QSize>

#include "VMVod.h"


class ScVodDataManager;
class ScUrlShareItem : public QObject
{
    Q_OBJECT
    Q_PROPERTY(qint64 urlShareId READ urlShareId CONSTANT)
    Q_PROPERTY(QString thumbnailFilePath READ thumbnailFilePath NOTIFY thumbnailPathChanged)
    Q_PROPERTY(QString vodFilePath READ vodFilePath NOTIFY vodFilePathChanged)
    Q_PROPERTY(quint64 vodFileSize READ vodFileSize NOTIFY vodFileSizeChanged)
    Q_PROPERTY(qreal downloadProgress READ downloadProgress NOTIFY downloadProgressChanged)
    Q_PROPERTY(QSize vodResolution READ vodResolution NOTIFY vodResolutionChanged)
    Q_PROPERTY(int vodLength READ vodLength NOTIFY vodLengthChanged)
    Q_PROPERTY(QString url READ url CONSTANT)
    Q_PROPERTY(FetchStatus metaDataFetchStatus READ metaDataFetchStatus NOTIFY metaDataFetchStatusChanged)
    Q_PROPERTY(FetchStatus thumbnailFetchStatus READ thumbnailFetchStatus NOTIFY thumbnailFetchStatusChanged)
    Q_PROPERTY(FetchStatus vodFetchStatus READ vodFetchStatus NOTIFY vodFetchStatusChanged)
    Q_PROPERTY(VMVod metaData READ metaData NOTIFY metaDataChanged)
    Q_PROPERTY(QString formatId READ formatId NOTIFY formatIdChanged)
    Q_PROPERTY(QString title READ title NOTIFY titleChanged)
public:
    enum FetchStatus
    {
        Unknown,
        Fetching,
        Failed,
        Available,
        Unavailable
    };
    Q_ENUMS(FetchStatus)

public:
    explicit ScUrlShareItem(qint64 urlSharedId, ScVodDataManager *parent);

public:
    QString title() const { return m_Title; }
    qint64 urlShareId() const { return m_UrlShareId; }
    QString thumbnailFilePath() const { return m_ThumbnailPath; }
    QString vodFilePath() const { return m_FilePath; }
    qreal downloadProgress() const { return m_Progress; }
    QSize vodResolution() const { return m_Size; }
    int vodLength() const { return m_Length; }
    QString url() const { return m_Url; }
    FetchStatus metaDataFetchStatus() const { return m_MetaDataFetchStatus; }
    FetchStatus thumbnailFetchStatus() const { return m_ThumbnailFetchStatus; }
    FetchStatus vodFetchStatus() const { return m_VodFetchStatus; }
    VMVod metaData() const { return m_MetaData; }
    QString formatId() const { return m_FormatId; }
    qint64 vodFileSize() const { return m_FileSize; }
    Q_INVOKABLE void fetchVod(int formatIndex);
    Q_INVOKABLE void fetchThumbnail();
    Q_INVOKABLE void fetchMetaData();


signals:
    void metaDataChanged();
    void thumbnailPathChanged();
    void vodFilePathChanged();
    void vodFileSizeChanged();
    void videoEndOffsetChanged();
    void downloadProgressChanged();
    void titleChanged();
    void vodLengthChanged();
    void metaDataFetchStatusChanged();
    void thumbnailFetchStatusChanged();
    void vodFetchStatusChanged();
    void vodResolutionChanged();
    void formatIdChanged();

private:
    struct Data
    {
        QString title;
        int length;

        Data() = default;
    };

public:
    void onTitleAvailable(const QString& title);
    void onMetaDataAvailable(const VMVod& vod);
    void onMetaDataUnavailable();
    void onFetchingMetaData();
    void onMetaDataDownloadFailed(int error);
    void onThumbnailAvailable(const QString& vodFilePath);
    void onThumbnailUnavailable();
    void onFetchingThumbnail();
    void onThumbnailDownloadFailed(int error, const QString& url);
    void onVodAvailable(
            const QString& vodFilePath,
            qreal progress,
            quint64 vodFileSize,
            int width,
            int height,
            const QString& formatId);
    void onVodUnavailable();
    void onVodDownloadCanceled();
    void onVodDownloadFailed(int error);

private slots:
    void reset();
    void onIsOnlineChanged();
//    void onTitleAvailable(qint64 urlShareId, QString title);
//    void onMetaDataAvailable(qint64 urlShareId, VMVod vod);
//    void onMetaDataUnavailable(qint64 urlShareId);
//    void onFetchingMetaData(qint64 urlShareId);
//    void onMetaDataDownloadFailed(qint64 urlShareId, int error);
//    void onThumbnailAvailable(qint64 urlShareId, QString vodFilePath);
//    void onThumbnailUnavailable(qint64 urlShareId);
//    void onFetchingThumbnail(qint64 urlShareId);
//    void onThumbnailFailed(qint64 urlShareId, int error, QString url);
//    void onVodAvailable(
//            qint64 urlShareId,
//            QString vodFilePath,
//            qreal progress,
//            quint64 vodFileSize,
//            int width,
//            int height,
//            QString formatId);
//    void onVodUnavailable(qint64 urlShareId);

private:
    ScVodDataManager* manager() const;
    bool fetch(Data* data) const;
    void setMetaData(const VMVod& value);
    void setThumbnailPath(const QString& path);
    void setMetaDataFetchStatus(FetchStatus value);
    void setThumbnailFetchStatus(FetchStatus value);
    void setVodFetchStatus(FetchStatus value);
    void setVodFilePath(const QString& value);
    void setDownloadProgress(float value);
    void setFilesize(qint64 value);
    void setFormatId(const QString& value);
    void setSize(const QSize& value);
    void setTitle(const QString& value);

private:
    VMVod m_MetaData;
    QString m_ThumbnailPath;
    QString m_FilePath;
    QString m_Title;
    QString m_Url;
    QString m_FormatId;
    qint64 m_UrlShareId;
    qint64 m_FileSize;
    qreal m_Progress;
    QSize m_Size;
    int m_Length;
    FetchStatus m_MetaDataFetchStatus;
    FetchStatus m_ThumbnailFetchStatus;
    FetchStatus m_VodFetchStatus;
    bool m_seen;
};


