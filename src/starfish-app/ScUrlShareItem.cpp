#include "ScUrlShareItem.h"
#include "ScVodDataManager.h"
#include "ScVodFileItem.h"

#include <QSqlQuery>
#include <QSqlError>
#include <QDebug>

ScUrlShareItem::Data::Data()
{
    length = 0;
    shareCount = 0;
}

ScUrlShareItem::ScUrlShareItem(qint64 urlShareId, ScVodDataManager *parent)
    : QObject(parent)
    , m_UrlShareId(urlShareId)
{
    connect(parent, &ScVodDataManager::vodsChanged, this, &ScUrlShareItem::reset);
    connect(parent, &ScVodDataManager::ytdlPathChanged, this, &ScUrlShareItem::reset);
    connect(parent, &ScVodDataManager::isOnlineChanged, this, &ScUrlShareItem::onIsOnlineChanged);
    connect(&m_MetaDataExpirationTimer, &QTimer::timeout, this, &ScUrlShareItem::onMetaDataExpired);

    QSqlQuery q(manager()->database());
    static const QString sql = QStringLiteral("SELECT url, video_id, type FROM vod_url_share WHERE id=?");
    if (Q_LIKELY(q.prepare(sql))) {
        q.addBindValue(m_UrlShareId);
        if (Q_LIKELY(q.exec())) {
            if (Q_LIKELY(q.next())) {
                m_Url = q.value(0).toString();
                if (m_Url.isEmpty()) {
                    m_Url = manager()->getUrl(q.value(2).toInt(), q.value(1).toString());
                }
            } else {
                qDebug() << "no data for url share id" << m_UrlShareId;
            }
        } else {
            qDebug() << "failed to exec, error:" << q.lastError();
        }
    } else {
        qDebug() << "failed to prepare, error:" << q.lastError();
    }

    reload();

    m_MetaDataExpirationTimer.setTimerType(Qt::CoarseTimer);
    m_MetaDataExpirationTimer.setSingleShot(true);
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
    static const QString sql = QStringLiteral("SELECT title, length, count(*) FROM url_share_vods WHERE vod_url_share_id=?");
    if (Q_LIKELY(q.prepare(sql))) {
        q.addBindValue(m_UrlShareId);
        if (Q_LIKELY(q.exec())) {
            if (Q_LIKELY(q.next())) {
                data->title = q.value(0).toString();
                data->length = q.value(1).toInt();
                data->shareCount = q.value(2).toInt();
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

void ScUrlShareItem::reload()
{
    setFileCount(0);

    m_Length = -1;
    emit vodLengthChanged();
    m_Progress = 0;
    emit downloadProgressChanged();
    m_FormatIndex = -1;
    emit vodFormatIndexChanged();
    m_ShareCount = -1;
    emit shareCountChanged();
    m_ThumbnailFetchStatus = Unknown;
    emit thumbnailFetchStatusChanged();
    m_MetaDataFetchStatus = Unknown;
    emit metaDataFetchStatusChanged();
    m_VodFetchStatus = Unknown;
    emit vodFetchStatusChanged();
    m_ThumbnailIsWaitingForMetaData = false;

    m_VodFetchStatus = Fetching;
    emit vodFetchStatusChanged();
    auto ticket = manager()->queryVodFiles(m_UrlShareId);
    if (-1 == ticket) {
        m_VodFetchStatus = Unknown;
        emit vodFetchStatusChanged();
    }

    reset();
}

void ScUrlShareItem::reset()
{
    updateUrlShareData(); // in case meta data isn't available was previously
    fetchMetaData(false);
    fetchThumbnail();
}

void
ScUrlShareItem::updateUrlShareData()
{
    Data data;
    if (fetch(&data)) {
        setTitle(data.title);

        if (m_Length != data.length) {
            m_Length = data.length;
            emit vodLengthChanged();
        }

        if (m_ShareCount != data.shareCount) {
            m_ShareCount = data.shareCount;
            emit shareCountChanged();
        }
    }
}

void
ScUrlShareItem::onIsOnlineChanged()
{
    if (manager()->isOnline()) {
        fetchMetaData(m_ThumbnailIsWaitingForMetaData);
        fetchThumbnail();
    }
}

void
ScUrlShareItem::setMetaDataFetchStatus(FetchStatus value)
{
    if (value != m_MetaDataFetchStatus) {
        m_MetaDataFetchStatus = value;
        qDebug() << m_UrlShareId << "meta data fetch status" << value;
        emit metaDataFetchStatusChanged();
    }
}

void
ScUrlShareItem::setThumbnailFetchStatus(FetchStatus value)
{
    if (value != m_ThumbnailFetchStatus) {
        m_ThumbnailFetchStatus = value;
        qDebug() << m_UrlShareId << "thumbnail fetch status" << value;
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
ScUrlShareItem::onMetaDataAvailable(const VMPlaylist& playlist, const QDateTime expirationDate)
{
    switch (m_MetaDataFetchStatus) {
    case Gone:
        break;
    default: {
        auto millis = QDateTime::currentDateTime().msecsTo(expirationDate);
        if (millis <= 0) { // meta data expired
            onMetaDataUnavailable();
        } else {
            m_MetaDataExpirationTimer.start(millis);
            setMetaData(playlist);
            setMetaDataFetchStatus(Available);
            updateUrlShareData(); // now that meta data is available try again for title, length, ...
            if (m_ThumbnailIsWaitingForMetaData) {
                fetchThumbnail();
            }
        }
    } break;
    }
}

void
ScUrlShareItem::onFetchingMetaData()
{
    switch (m_MetaDataFetchStatus) {
    case Gone:
        break;
    default:
        setMetaDataFetchStatus(Fetching);
        break;
    }
}

void
ScUrlShareItem::onMetaDataUnavailable()
{
    switch (m_MetaDataFetchStatus) {
    case Gone:
        break;
    default:
        setMetaDataFetchStatus(Unavailable);
        break;
    }
}

void
ScUrlShareItem::onMetaDataDownloadFailed(VMVodEnums::Error error)
{
    switch (m_MetaDataFetchStatus) {
    case Gone:
        break;
    default:
        setMetaDataFetchStatus(error == VMVodEnums::VM_ErrorContentGone ? Gone : Failed);
        break;
    }
}

void
ScUrlShareItem::onThumbnailAvailable(const QString& filePath)
{
    switch (m_ThumbnailFetchStatus) {
    case Gone:
        break;
    default:
        m_ThumbnailIsWaitingForMetaData = false;
        setThumbnailPath(filePath);
        setThumbnailFetchStatus(Available);
        break;
    }
}

void
ScUrlShareItem::onFetchingThumbnail()
{
    switch (m_ThumbnailFetchStatus) {
    case Gone:
        break;
    default:
        m_ThumbnailIsWaitingForMetaData = false;
        setThumbnailFetchStatus(Fetching);
        break;
    }
}

void
ScUrlShareItem::onThumbnailUnavailable(ScVodDataManagerWorker::ThumbnailError error)
{
    switch (m_ThumbnailFetchStatus) {
    case Gone:
        break;
    default:
        switch (error) {
        case ScVodDataManagerWorker::TE_ContentGone:
            m_ThumbnailIsWaitingForMetaData = false;
            setThumbnailFetchStatus(Gone);
            break;
        case ScVodDataManagerWorker::TE_MetaDataUnavailable:
            m_ThumbnailIsWaitingForMetaData = true;
            setThumbnailFetchStatus(Unavailable);
            // no/invalid meta data, fetch so we can show the thumbnail
            fetchMetaData(true);
            break;
        default:
            m_ThumbnailIsWaitingForMetaData = false;
            setThumbnailFetchStatus(Unavailable);
            break;
        }
    }
}

void
ScUrlShareItem::onThumbnailDownloadFailed(int error, const QString& url)
{
    Q_UNUSED(error);
    Q_UNUSED(url);

    switch (m_ThumbnailFetchStatus) {
    case Gone:
        break;
    default:
        setThumbnailFetchStatus(Failed);
        break;
    }
}


void
ScUrlShareItem::onVodAvailable(const ScVodFileFetchProgress& progress)
{
    onVodAvailable(progress, Available);
}

void
ScUrlShareItem::onFetchingVod(const ScVodFileFetchProgress& progress)
{
    onVodAvailable(progress, Fetching);
}

void
ScUrlShareItem::onVodAvailable(const ScVodFileFetchProgress& progress, FetchStatus status)
{
    if (progress.fileCount < 0) {
        qWarning() << "invalid file count" << progress.fileCount;
        return;
    }

    if (progress.fileCount > 0 &&
            (progress.fileIndex < 0 || progress.fileIndex >= progress.fileCount)) {
        qWarning() << "invalid file index" << progress.fileIndex;
        return;
    }

    setFileCount(progress.fileCount);

    setSize(QSize(progress.width, progress.height));
    setFormatId(progress.formatId);
    setVodFetchStatus(status);

    if (progress.fileCount) {
        m_Files[progress.fileIndex]->onVodAvailable(progress);
    }
}

void
ScUrlShareItem::onVodUnavailable()
{
    setFormatIndex(-1);
    setVodFetchStatus(Unavailable);
}

void
ScUrlShareItem::onVodDownloadCanceled()
{
    if (m_Files.size() >= 1 &&
            m_Files[0]->vodFileSize() > 0 &&
            !m_Files[0]->vodFilePath().isEmpty()) {
        setVodFetchStatus(Available);
    } else {
        setVodFetchStatus(Unavailable);
    }
}

void
ScUrlShareItem::onVodDownloadFailed(int error)
{
    Q_UNUSED(error);
    setFormatIndex(-1);
    setVodFetchStatus(Failed);
}

void
ScUrlShareItem::onTitleAvailable(const QString& title)
{
    setTitle(title);
}


void
ScUrlShareItem::setThumbnailPath(const QString& value)
{
    if (value != m_ThumbnailPath) {
        m_ThumbnailPath = value;
        emit thumbnailPathChanged();
    }
}

void
ScUrlShareItem::setMetaData(const VMPlaylist& value)
{
    m_MetaData = value;
    emit metaDataChanged();

    updateFormatIndex();

//    fetchThumbnail();
}


void
ScUrlShareItem::setFormatIndex(int value)
{
    if (value != m_FormatIndex) {
        m_FormatIndex = value;
        emit vodFormatIndexChanged();
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
    m_FormatId = value;
    updateFormatIndex();

//    if (value != m_FormatId) {
//        m_FormatId = value;
//        emit formatIdChanged();

//        updateFormatIndex();
//    }
}

void
ScUrlShareItem::fetchVodFiles(int formatIndex)
{
    if (!m_MetaData.isValid()) {
        qWarning() << "no meta data";
        return;
    }

    if (formatIndex < 0 ||
            formatIndex >= m_MetaData._vods()[0].avFormats()) {
        qWarning() << "invalid format index" << formatIndex;
        return;
    }

    const auto& format = m_MetaData._vods()[0]._avFormats()[formatIndex];


    switch (m_VodFetchStatus) {
    case Available:
        if (m_FormatIndex == formatIndex && downloadProgress() >= 1) {
            qDebug() << "fully downloaded";
            return;
        }
    case Unknown:
    case Failed:
    case Unavailable: {
        if (manager()->isOnline()) {
            auto last = m_VodFetchStatus;
            setVodFetchStatus(Fetching);
            auto result = manager()->fetchVod(m_UrlShareId, format.id());
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
    fetchMetaData(true);
}

void
ScUrlShareItem::fetchMetaData(bool download)
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
        auto result = manager()->fetchMetaData(m_UrlShareId, m_Url, download && manager()->isOnline());
        if (-1 == result) {
            setMetaDataFetchStatus(last);
        }
    } break;
    default:
        // implicit handle of gone
        break;
    }
}

void
ScUrlShareItem::deleteMetaData()
{
    switch (m_MetaDataFetchStatus) {
    case Gone:
        break;
    default:
        manager()->deleteMetaData(m_UrlShareId);
//        setMetaData(VMVod());
        setMetaDataFetchStatus(Unavailable);
        m_MetaDataExpirationTimer.stop();
        break;
    }
}

void
ScUrlShareItem::deleteVodFiles()
{
    switch (m_VodFetchStatus) {
    case Available:
        manager()->deleteVodFiles(m_UrlShareId);
        // set all of these to give QML a better chance to show/hide menu items
        setFileCount(0);
        setSize(QSize());
        setFormatId(QString());
        setFormatIndex(-1);
        setVodFetchStatus(Unavailable);
        break;
    default:
        break;
    }
}

void
ScUrlShareItem::cancelFetchVodFiles()
{
    switch (m_VodFetchStatus) {
    case Fetching:
        manager()->cancelFetchVod(m_UrlShareId);
        break;
    default:
        break;
    }
}

void
ScUrlShareItem::deleteThumbnail()
{
    switch (m_ThumbnailFetchStatus) {
    case Gone:
        break;
    default:
        manager()->deleteThumbnail(m_UrlShareId);
        setThumbnailFetchStatus(Unavailable);
        m_ThumbnailIsWaitingForMetaData = false;
        break;
    }
}

void
ScUrlShareItem::updateFormatIndex()
{
    if (m_MetaData.isValid() && !m_FormatId.isEmpty()) {
        auto result = -1;
        const auto& fs = m_MetaData._vods()[0]._avFormats();
        for (auto i = 0; i < fs.size(); ++i) {
            if (fs[i].id() == m_FormatId) {
                result = i;
                break;
            }
        }
        setFormatIndex(result);
    } else {
        setFormatIndex(-1);
    }
}

ScVodFileItem*
ScUrlShareItem::file(int index) const
{
    if (index >= 0 && index < m_Files.size()) {
        return m_Files[index];
    }

    qCritical() << "index out of range" << index;
    return nullptr;
}

void
ScUrlShareItem::setFileCount(int count)
{
    if (m_Files.size() != count) {
        qDebug() << m_UrlShareId << "change file count to" << count;

        while (m_Files.size() > count) {
            delete m_Files.last();
            m_Files.pop_back();
        }

        m_Files.reserve(count);

        while (m_Files.size() < count) {
            auto file = new ScVodFileItem(this);
            connect(file, &ScVodFileItem::downloadProgressChanged, this, &ScUrlShareItem::updateDownloadProgress);
            connect(file, &ScVodFileItem::durationChanged, this, &ScUrlShareItem::updateDownloadProgress);
            m_Files << file;
        }

        emit filesChanged();
        updateDownloadProgress();
    }
}

void
ScUrlShareItem::updateDownloadProgress()
{
    qreal progress = 0;
    if (m_Length > 0) {
        for (auto file : m_Files) {
            progress += file->downloadProgress() * file->duration();
        }

        progress /= m_Length;
        progress = qMax(qreal{0}, qMin(progress, qreal{1}));
    }

    setDownloadProgress(progress);
}

void
ScUrlShareItem::setDownloadProgress(qreal value)
{
    if (value == value && value != m_Progress) {
        m_Progress = value;
        emit downloadProgressChanged();
    }
}

void
ScUrlShareItem::onMetaDataExpired()
{
    if (Available == m_MetaDataFetchStatus) {
        setMetaDataFetchStatus(Unavailable);
    }
}
