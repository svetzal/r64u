#pragma once
#include <QNetworkReply>

namespace networkerror {
inline bool isConnectionError(QNetworkReply::NetworkError error)
{
    return error == QNetworkReply::ConnectionRefusedError ||
           error == QNetworkReply::HostNotFoundError || error == QNetworkReply::TimeoutError;
}
}  // namespace networkerror
