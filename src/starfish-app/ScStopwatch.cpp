#include "ScStopwatch.h"

#include <QDateTime>
#include <QLoggingCategory>

Q_LOGGING_CATEGORY(perf, "perf")

ScStopwatch::~ScStopwatch() {
    auto stop = QDateTime::currentMSecsSinceEpoch();
    auto duration = stop - m_Start;
    if (duration > m_Threshold) {
        qWarning(perf, "%s took %d [ms]\n", qPrintable(m_Name), (int)duration);
    } else {
//            qDebug("%s took %d [ms]\n", qPrintable(m_Name), (int)duration);
    }
}

ScStopwatch::ScStopwatch(const char* name, int millisBeforeWarn)
    : m_Name(QLatin1String(name))
    , m_Start(QDateTime::currentMSecsSinceEpoch())
    , m_Threshold(millisBeforeWarn)
{

}

ScStopwatch::ScStopwatch(const QString& str, int millisBeforeWarn)
    : m_Name(str)
    , m_Start(QDateTime::currentMSecsSinceEpoch())
    , m_Threshold(millisBeforeWarn)
{

}
