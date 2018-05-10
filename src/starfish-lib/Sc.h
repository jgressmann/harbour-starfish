#pragma once

#include <QString>
#include <QNetworkRequest>

class QUrl;
struct Sc
{
    static const QByteArray UserAgent;
    static QNetworkRequest makeRequest(const QUrl& url);
};
