#include "ScUrlShareItem.h"
#include "ScVodDataManager.h"

#include <QSqlQuery>
#include <QSqlError>
#include <QDebug>

namespace
{

} // anon

ScUrlShareItem::ScUrlShareItem(qint64 urlShareId, ScVodDataManager *parent)
    : QObject(parent)
    , m_UrlShareId(urlShareId)
{
    m_FileSize = -1;
    m_Length = -1;
    m_Progress = -1;
    m_ThumbnailFetchStatus = Unknown;
    m_MetaDataFetchStatus = Unknown;
    m_VodFetchStatus = Unknown;

    connect(parent, &ScVodDataManager::vodsChanged, this, &ScUrlShareItem::reset);
    connect(parent, &ScVodDataManager::ytdlPathChanged, this, &ScUrlShareItem::reset);
    connect(parent, &ScVodDataManager::isOnlineChanged, this, &ScUrlShareItem::onIsOnlineChanged);
//    connect(parent, &ScVodDataManager::titleAvailable, this, &ScUrlShareItem::onTitleAvailable);
//    connect(parent, &ScVodDataManager::metaDataAvailable, this, &ScUrlShareItem::onMetaDataAvailable);
//    connect(parent, &ScVodDataManager::metaDataUnavailable, this, &ScUrlShareItem::onMetaDataUnavailable);
//    connect(parent, &ScVodDataManager::metaDataDownloadFailed, this, &ScUrlShareItem::onMetaDataDownloadFailed);
//    connect(parent, &ScVodDataManager::thumbnailAvailable, this, &ScUrlShareItem::onThumbnailAvailable);
//    connect(parent, &ScVodDataManager::thumbnailUnavailable, this, &ScUrlShareItem::onThumbnailUnavailable);
//    connect(parent, &ScVodDataManager::thumbnailDownloadFailed, this, &ScUrlShareItem::onThumbnailFailed);
//    connect(parent, &ScVodDataManager::vodAvailable, this, &ScUrlShareItem::onVodAvailable);
//    connect(parent, &ScVodDataManager::vodUnavailable, this, &ScUrlShareItem::onVodUnavailable);
//    connect(parent, &ScVodDataManager::fetchingMetaData, this, &ScUrlShareItem::onFetchingMetaData);
//    connect(parent, &ScVodDataManager::fetchingThumbnail, this, &ScUrlShareItem::onFetchingThumbnail);


    QSqlQuery q(manager()->database());
    auto sql = QStringLiteral("SELECT url FROM vod_url_share WHERE id=?");
    if (q.prepare(sql)) {
        q.addBindValue(m_UrlShareId);
        if (q.exec()) {
            if (q.next()) {
                m_Url = q.value(0).toString();
            } else {
                qDebug() << "no data for url share id" << m_UrlShareId;
            }
        } else {
            qDebug() << "failed to exec, error:" << q.lastError();
        }
    } else {
        qDebug() << "failed to prepare, error:" << q.lastError();
    }

    m_VodFetchStatus = Fetching;
    auto ticket = manager()->queryVodFiles(m_UrlShareId);
    if (-1 == ticket) {
        m_VodFetchStatus = Unknown;
    }

    reset();
}

ScVodDataManager*
ScUrlShareItem::manager() const
{
    return qobject_cast<ScVodDataManager*>(parent());
}

bool ScUrlShareItem::fetch(Data* data) const
{
    Q_ASSERT(data);
    QSqlQuery q(manager()->database());
    auto sql = QStringLiteral("SELECT title, length FROM vod_url_share WHERE id=?");
    if (q.prepare(sql)) {
        q.addBindValue(m_UrlShareId);
        if (q.exec()) {
            if (q.next()) {
                data->title = q.value(0).toString();
                data->length = q.value(1).toInt();
                return true;
            } else {
                qDebug() << "no data for urlShareId" << m_UrlShareId;
            }
        } else {
            qDebug() << "failed to exec, error:" << q.lastError();
        }
    } else {
        qDebug() << "failed to prepare, error:" << q.lastError();
    }

    return false;
}


void ScUrlShareItem::reset()
{
    Data data;
    if (fetch(&data)) {
        setTitle(data.title);
        if (m_Title != data.title) {
            m_Title = data.title;
            emit titleChanged();
        }

        if (m_Length != data.length) {
            m_Length = data.length;
            emit vodLengthChanged();
        }
    }

    onIsOnlineChanged();
}


void
ScUrlShareItem::onIsOnlineChanged()
{
    fetchMetaData();
    fetchThumbnail();
}

void
ScUrlShareItem::setMetaDataFetchStatus(FetchStatus value)
{
    if (value != m_MetaDataFetchStatus) {
        m_MetaDataFetchStatus = value;
        emit metaDataFetchStatusChanged();
    }
}

void
ScUrlShareItem::setThumbnailFetchStatus(FetchStatus value)
{
    if (value != m_ThumbnailFetchStatus) {
        m_ThumbnailFetchStatus = value;
        emit thumbnailFetchStatusChanged();
    }
}

void
ScUrlShareItem::setVodFetchStatus(FetchStatus value)
{
    if (value != m_VodFetchStatus) {
        m_VodFetchStatus = value;
        emit vodFetchStatusChanged();
    }
}

void
ScUrlShareItem::onMetaDataAvailable(const VMVod& vod)
{
    setMetaData(vod);
    setMetaDataFetchStatus(Available);
}

void
ScUrlShareItem::onFetchingMetaData() {
    setMetaDataFetchStatus(Fetching);
}

void
ScUrlShareItem::onMetaDataUnavailable()
{
    setMetaData(VMVod());
    setMetaDataFetchStatus(Unavailable);
}

void
ScUrlShareItem::onMetaDataDownloadFailed(int error)
{
    Q_UNUSED(error);
    setMetaData(VMVod());
    setMetaDataFetchStatus(Failed);
}

void
ScUrlShareItem::onThumbnailAvailable(const QString& filePath)
{
    setThumbnailPath(filePath);
    setThumbnailFetchStatus(Available);
}

void
ScUrlShareItem::onFetchingThumbnail() {
    setThumbnailFetchStatus(Fetching);
}

void
ScUrlShareItem::onThumbnailUnavailable()
{
    setThumbnailPath(QString());
    setThumbnailFetchStatus(Unavailable);
}

void
ScUrlShareItem::onThumbnailDownloadFailed(int error, const QString& url)
{
    Q_UNUSED(error);
    Q_UNUSED(url);
    setThumbnailPath(QString());
    setThumbnailFetchStatus(Failed);
}


void
ScUrlShareItem::onVodAvailable(
        const QString& filePath,
        qreal progress,
        quint64 fileSize,
        int width,
        int height,
        const QString& formatId) {

    setVodFilePath(filePath);
    setDownloadProgress(progress);
    setFilesize(fileSize);
    setSize(QSize(width, height));
    setFormatId(formatId);
    setVodFetchStatus(Available);
}

void
ScUrlShareItem::onVodUnavailable()
{
    setVodFetchStatus(Unavailable);
}

void
ScUrlShareItem::onVodDownloadCanceled()
{


}

void
ScUrlShareItem::onVodDownloadFailed(int error)
{
    Q_UNUSED(error);
    setVodFetchStatus(Failed);
}

void
ScUrlShareItem::onTitleAvailable(const QString& title)
{
    setTitle(title);
}


//void ScUrlShareItem::onTitleAvailable(qint64 urlShareId, QString title)
//{
//    if (urlShareId == m_UrlShareId) {
//        if (m_Title != title) {
//            m_Title = title;
//            emit titleChanged();
//        }
//    }
//}


//void
//ScUrlShareItem::onMetaDataAvailable(qint64 urlShareId, VMVod vod)
//{
//    if (urlShareId == m_UrlShareId) {
//        setMetaData(std::move(vod));
//        setMetaDataFetchStatus(Available);
//    }
//}

//void
//ScUrlShareItem::onFetchingMetaData(qint64 urlShareId) {
//    if (urlShareId == m_UrlShareId) {
//        setMetaDataFetchStatus(Fetching);
//    }
//}

//void
//ScUrlShareItem::onMetaDataUnavailable(qint64 urlShareId)
//{
//    if (urlShareId == m_UrlShareId) {
//        setMetaDataFetchStatus(Unavailable);
//    }
//}

//void
//ScUrlShareItem::onMetaDataDownloadFailed(qint64 urlShareId, int error)
//{
//    Q_UNUSED(error);
//    if (urlShareId == m_UrlShareId) {
//        setMetaDataFetchStatus(Failed);
//    }
//}

//void
//ScUrlShareItem::onThumbnailAvailable(qint64 urlShareId, QString filePath)
//{
//    if (urlShareId == m_UrlShareId) {
//        setThumbnailPath(std::move(filePath));
//        setThumbnailFetchStatus(Available);
//    }
//}

//void
//ScUrlShareItem::onFetchingThumbnail(qint64 urlShareId) {
//    if (urlShareId == m_UrlShareId) {
//        setThumbnailFetchStatus(Fetching);
//    }
//}

//void
//ScUrlShareItem::onThumbnailUnavailable(qint64 urlShareId)
//{
//    if (urlShareId == m_UrlShareId) {
//        setThumbnailFetchStatus(Unavailable);
//    }
//}

//void
//ScUrlShareItem::onThumbnailFailed(qint64 urlShareId, int error, QString url)
//{
//    Q_UNUSED(error);
//    Q_UNUSED(url);
//    if (urlShareId == m_UrlShareId) {
//        setThumbnailFetchStatus(Failed);
//    }
//}


//void
//ScUrlShareItem::onVodAvailable(
//        qint64 urlShareId,
//        QString filePath,
//        qreal progress,
//        quint64 fileSize,
//        int width,
//        int height,
//        QString formatId) {
//    if (urlShareId == m_UrlShareId) {
//        setVodFilePath(std::move(filePath));
//        setDownloadProgress(progress);
//        setFilesize(fileSize);
//        setSize(QSize(width, height));
//        setFormatId(std::move(formatId));
//        setVodFetchStatus(Available);
//    }
//}

//void
//ScUrlShareItem::onVodUnavailable(qint64 urlShareId) {
//    if (urlShareId == m_UrlShareId) {
//        setVodFetchStatus(Unavailable);
//    }
//}


void
ScUrlShareItem::setThumbnailPath(const QString& value)
{
    if (value != m_ThumbnailPath) {
        m_ThumbnailPath = value;
        emit thumbnailPathChanged();
    }
}

void
ScUrlShareItem::setMetaData(const VMVod& value)
{
    m_MetaData = value;
    emit metaDataChanged();

    fetchThumbnail();
}

void
ScUrlShareItem::setVodFilePath(const QString& value)
{
    if (value != m_FilePath) {
        m_FilePath = std::move(value);
        emit vodFilePathChanged();
    }
}

void
ScUrlShareItem::setDownloadProgress(float value)
{
    m_Progress = value;
    emit downloadProgressChanged();
}

void
ScUrlShareItem::setFilesize(qint64 value)
{
    if (value != m_FileSize) {
        m_FileSize = value;
        emit vodFileSizeChanged();
    }
}

void
ScUrlShareItem::setTitle(const QString& value)
{
    if (value != m_Title) {
        m_Title = value;
        emit titleChanged();
    }
}

void
ScUrlShareItem::setSize(const QSize& value)
{
    if (value != m_Size) {
        m_Size = value;
        emit vodResolutionChanged();
    }
}

void
ScUrlShareItem::setFormatId(const QString& value)
{
    if (value != m_FormatId) {
        m_FormatId = value;
        emit formatIdChanged();
    }
}

void
ScUrlShareItem::fetchVod(int formatIndex)
{
    switch (m_VodFetchStatus) {
    case Available:
        if (formatIndex < 0 || formatIndex >= m_MetaData.formats()) {
            qWarning() << "invalid format index" << formatIndex;
            return;
        }

        if (m_MetaData._formats()[formatIndex].id() == m_FormatId) {
            // already available
            return;
        }
    case Unknown:
    case Failed:
    case Unavailable:{
        if (manager()->isOnline()) {
            auto last = m_VodFetchStatus;
            setVodFetchStatus(Fetching);
            auto result = manager()->fetchVod(m_UrlShareId, formatIndex);
            if (-1 == result) {
                setVodFetchStatus(last);
            }
        }
    } break;
    default:
        break;
    }
}

void
ScUrlShareItem::fetchThumbnail()
{

    switch (m_ThumbnailFetchStatus) {
//    case Unavailable:
//        if (Available == m_MetaDataFetchStatus && manager()->isOnline()) {
//            auto last = m_ThumbnailFetchStatus;
//            setThumbnailFetchStatus(Fetching);
//            auto result = manager()->fetchThumbnail(m_UrlShareId, true);
//            if (-1 == result) {
//                setThumbnailFetchStatus(last);
//            }
//        }
//        break;
    case Unavailable:
    case Unknown:
    case Failed: {
//        if (Available == m_MetaDataFetchStatus) {
            auto last = m_ThumbnailFetchStatus;
            setThumbnailFetchStatus(Fetching);
            auto result = manager()->fetchThumbnail(m_UrlShareId, manager()->isOnline());
            if (-1 == result) {
                setThumbnailFetchStatus(last);
            }
//        }
    } break;
    default:
        break;
    }
}

void
ScUrlShareItem::fetchMetaData()
{
    switch (m_MetaDataFetchStatus) {
//    case Unavailable:
//        if (manager()->isOnline()) {
//            auto last = m_MetaDataFetchStatus;
//            setMetaDataFetchStatus(Fetching);
//            auto result = manager()->fetchMetaData(m_UrlShareId, m_Url, true);
//            if (-1 == result) {
//                setMetaDataFetchStatus(last);
//            }
//        }
//        break;
    case Unavailable:
    case Unknown:
    case Failed: {
        auto last = m_MetaDataFetchStatus;
        setMetaDataFetchStatus(Fetching);
        auto result = manager()->fetchMetaData(m_UrlShareId, m_Url, manager()->isOnline());
        if (-1 == result) {
            setMetaDataFetchStatus(last);
        }
    } break;
    default:
        break;
    }
}
