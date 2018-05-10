/* The MIT License (MIT)
 *
 * Copyright (c) 2018 Jean Gressmann <jean@0x42.de>
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

#include "Vods.h"
#include "ScRecord.h"

#include <QMutex>
#include <QNetworkAccessManager>

class ScClassifier;
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
    inline ScClassifier* classifier() const { return m_Classifier; }
    inline void setClassifier(ScClassifier* value) { m_Classifier = value; }

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
    ScClassifier* m_Classifier;
};

