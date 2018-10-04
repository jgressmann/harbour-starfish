#include "ScStopwatch.h"

#include <QDateTime>
#include <QLoggingCategory>

Q_LOGGING_CATEGORY(perf, "perf")

ScStopwatch::~ScStopwatch() {
    stop();
}

ScStopwatch::ScStopwatch(const char* name, int millisBeforeWarn)
    : m_Name(QLatin1String(name))
    , m_Threshold(millisBeforeWarn)
{
    start();
}

ScStopwatch::ScStopwatch(const QString& str, int millisBeforeWarn)
    : m_Name(str)
    , m_Threshold(millisBeforeWarn)
{
    start();
}

void ScStopwatch::start() {
    m_Start = QDateTime::currentMSecsSinceEpoch();
}

void ScStopwatch::stop() {
    if (m_Start) {
        auto stop = QDateTime::currentMSecsSinceEpoch();
        auto duration = stop - m_Start;
        m_Start = 0;
        if (duration > m_Threshold) {
            qWarning(perf, "%s took %d [ms]\n", qPrintable(m_Name), (int)duration);
        } else {
    //            qDebug("%s took %d [ms]\n", qPrintable(m_Name), (int)duration);
        }
    }
}
