#pragma once

#include "Vods.h"
#include "ScRecord.h"

#include <QMutex>
#include <QNetworkAccessManager>

class ScVodScraper : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool canCancelFetch READ canCancelFetch NOTIFY canCancelFetchChanged)
    Q_PROPERTY(bool canStartFetch READ canStartFetch NOTIFY canStartFetchChanged)
    Q_PROPERTY(bool canSkip READ canSkip CONSTANT)
    Q_PROPERTY(qreal progress READ progress NOTIFY progressChanged)
    Q_PROPERTY(QString progressDescription READ progressDescription NOTIFY progressDescriptionChanged)
    Q_PROPERTY(Status status READ status NOTIFY statusChanged)
    Q_PROPERTY(Error error READ error NOTIFY errorChanged)
public:
    enum Status {
        Status_Ready,
        Status_VodFetchingInProgress,
        Status_VodFetchingComplete,
        Status_VodFetchingBeingCanceled,
        Status_VodFetchingCanceled,
        Status_Error,
    };
    Q_ENUMS(Status)

    enum Error {
        Error_None,
        Error_NetworkFailure,
    };
    Q_ENUMS(Error)

public:
    virtual ~ScVodScraper();
    ScVodScraper(QObject* parent = Q_NULLPTR);


public:
    QNetworkAccessManager* networkAccessManager() const { return &m_Manager; }
    Status status() const { return m_Status; }
    Error error() const { return m_Error; }
    qreal progress() const { return m_Progress; }
    QString progressDescription() const { return m_ProgressDescription; }
    virtual QList<ScRecord> vods() const = 0;
    bool canCancelFetch() const;
    bool canStartFetch() const;
    virtual bool canSkip() const;

signals:
    void statusChanged();
    void errorChanged();
    void progressChanged();
    void progressDescriptionChanged();
    void canCancelFetchChanged();
    void canStartFetchChanged();
    void excludeEvent(const ScEvent& event, bool* exclude);
    void excludeStage(const ScStage& stage, bool* exclude);
    void excludeMatch(const ScMatch& match, bool* exclude);
    void excludeRecord(const ScRecord& record, bool* exclude);
    void hasRecord(const ScRecord& record, bool* exists);


public slots:
    void startFetch();
    void cancelFetch();

protected:
    virtual void _fetch() = 0;
    virtual void _cancel();
    void setStatus(Status value);
    void setError(Error error);
    void setProgress(qreal value);
    void setProgressDescription(const QString& value);
    QMutex* lock() const;

private:
    mutable QMutex m_Lock;
    mutable QNetworkAccessManager m_Manager;
    QString m_ProgressDescription;
    qreal m_Progress;
    Status m_Status;
    Error m_Error;
};

