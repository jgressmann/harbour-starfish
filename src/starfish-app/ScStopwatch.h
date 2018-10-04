#pragma once

#include <QString>

class ScStopwatch {
public:
    ~ScStopwatch();
    ScStopwatch(const char* name, int millisBeforeWarn = 100);
    ScStopwatch(const QString& str, int millisBeforeWarn = 100);

    ScStopwatch(const ScStopwatch&) = delete;
    ScStopwatch& operator=(const ScStopwatch&) = delete;
    void start();
    void stop();

private:
    QString m_Name;
    qint64 m_Start;
    int m_Threshold;
};

