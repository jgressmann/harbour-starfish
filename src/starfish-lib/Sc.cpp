#include "Sc.h"



const QByteArray Sc::UserAgent(STARFISH_APP_NAME QT_STRINGIFY(STARFISH_VERSION_MAJOR) "." QT_STRINGIFY(STARFISH_VERSION_MINOR) "." QT_STRINGIFY(STARFISH_VERSION_PATCH));

QNetworkRequest
Sc::makeRequest(const QUrl& url) {
    QNetworkRequest request;
    request.setUrl(url);
    request.setRawHeader("User-Agent", UserAgent); // must be set else no reply

    return request;
}
