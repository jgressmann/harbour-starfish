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

#include "VMPlaylist.h"
#include "ScVodDataManagerWorker.h"

class ScVodFileItem;
class ScVodDataManager;
class ScUrlShareItem : public QObject
{
    Q_OBJECT
    Q_PROPERTY(qint64 urlShareId READ urlShareId CONSTANT)
    Q_PROPERTY(QString thumbnailFilePath READ thumbnailFilePath NOTIFY thumbnailPathChanged)
    Q_PROPERTY(qreal downloadProgress READ downloadProgress NOTIFY downloadProgressChanged)
    Q_PROPERTY(QSize vodResolution READ vodResolution NOTIFY vodResolutionChanged)
    Q_PROPERTY(int vodLength READ vodLength NOTIFY vodLengthChanged)
    Q_PROPERTY(QString url READ url CONSTANT)
    Q_PROPERTY(FetchStatus metaDataFetchStatus READ metaDataFetchStatus NOTIFY metaDataFetchStatusChanged)
    Q_PROPERTY(FetchStatus thumbnailFetchStatus READ thumbnailFetchStatus NOTIFY thumbnailFetchStatusChanged)
    Q_PROPERTY(FetchStatus vodFetchStatus READ vodFetchStatus NOTIFY vodFetchStatusChanged)
    Q_PROPERTY(VMPlaylist metaData READ metaData NOTIFY metaDataChanged)
    Q_PROPERTY(QString title READ title NOTIFY titleChanged)
//    Q_PROPERTY(QString vodFormatId READ vodFormatId NOTIFY vodFormatIdChanged)
    Q_PROPERTY(int vodFormatIndex READ vodFormatIndex NOTIFY vodFormatIndexChanged)
    Q_PROPERTY(int shareCount READ shareCount NOTIFY shareCountChanged)
    Q_PROPERTY(int files READ files NOTIFY filesChanged)

public:
    enum FetchStatus
    {
        Unknown,
        Fetching,
        Failed,
        Available,
        Unavailable,
        Gone
    };
    Q_ENUMS(FetchStatus)

public:
    explicit ScUrlShareItem(qint64 urlSharedId, ScVodDataManager *parent);

public:
    QString title() const { return m_Title; }
    qint64 urlShareId() const { return m_UrlShareId; }
    QString thumbnailFilePath() const { return m_ThumbnailPath; }
    qreal downloadProgress() const { return m_Progress; }
    QSize vodResolution() const { return m_Size; }
    int vodLength() const { return m_Length; }
    QString url() const { return m_Url; }
    FetchStatus metaDataFetchStatus() const { return m_MetaDataFetchStatus; }
    FetchStatus thumbnailFetchStatus() const { return m_ThumbnailFetchStatus; }
    FetchStatus vodFetchStatus() const { return m_VodFetchStatus; }
    VMPlaylist metaData() const { return m_MetaData; }
    int vodFormatIndex() const { return m_FormatIndex; }
//    QString vodFormatId() const { return m_FormatId; }
    int shareCount() const { return m_ShareCount; }
    int files() const { return m_Files.size(); }
    Q_INVOKABLE void fetchVodFiles(int formatIndex);
    Q_INVOKABLE void cancelFetchVodFiles();
    Q_INVOKABLE void fetchThumbnail();
    Q_INVOKABLE void fetchMetaData();
    Q_INVOKABLE void deleteMetaData();
    Q_INVOKABLE void deleteThumbnail();
    Q_INVOKABLE void deleteVodFiles();
    Q_INVOKABLE ScVodFileItem* file(int index) const;


signals:
    void metaDataChanged();
    void thumbnailPathChanged();
    void videoEndOffsetChanged();
    void downloadProgressChanged();
    void titleChanged();
    void vodLengthChanged();
    void metaDataFetchStatusChanged();
    void thumbnailFetchStatusChanged();
    void vodFetchStatusChanged();
    void vodResolutionChanged();
    void vodFormatIndexChanged();
//    void vodFormatIdChanged();
    void shareCountChanged();
    void filesChanged();

private:
    struct Data
    {
        QString title;
        int length;
        int shareCount;

        Data() = default;
    };

public:
    void onTitleAvailable(const QString& title);
    void onMetaDataAvailable(const VMPlaylist& playlist);
    void onMetaDataUnavailable();
    void onFetchingMetaData();
    void onMetaDataDownloadFailed(VMVodEnums::Error error);
    void onThumbnailAvailable(const QString& vodFilePath);
    void onThumbnailUnavailable(ScVodDataManagerWorker::ThumbnailError error);
    void onFetchingThumbnail();
    void onThumbnailDownloadFailed(int error, const QString& url);
    void onVodAvailable(const ScVodFileFetchProgress& progress);
    void onFetchingVod(const ScVodFileFetchProgress& progress);
    void onVodUnavailable();
    void onVodDownloadCanceled();
    void onVodDownloadFailed(int error);

private slots:
    void reset();
    void onIsOnlineChanged();

private:
    ScVodDataManager* manager() const;
    bool fetch(Data* data) const;
    void setMetaData(const VMPlaylist& value);
    void setThumbnailPath(const QString& path);
    void setMetaDataFetchStatus(FetchStatus value);
    void setThumbnailFetchStatus(FetchStatus value);
    void setVodFetchStatus(FetchStatus value);
    void setFormatId(const QString& value);
    void setSize(const QSize& value);
    void setTitle(const QString& value);
    void setFormatIndex(int value);
    void updateFormatIndex();
    void fetchMetaData(bool download);
    void updateUrlShareData();
    void onVodAvailable(const ScVodFileFetchProgress& progress, FetchStatus status);
    void setFileCount(int count);
    void updateDownloadProgress();
    void setDownloadProgress(qreal value);

private:
    QVector<ScVodFileItem*> m_Files;
    VMPlaylist m_MetaData;
    QString m_ThumbnailPath;
    QString m_Title;
    QString m_Url;
    QString m_FormatId;
    qint64 m_UrlShareId;
    QSize m_Size;
    qreal m_Progress;
    int m_FormatIndex;
    int m_Length;
    int m_ShareCount;
    FetchStatus m_MetaDataFetchStatus;
    FetchStatus m_ThumbnailFetchStatus;
    FetchStatus m_VodFetchStatus;
    bool m_ThumbnailIsWaitingForMetaData;
};


