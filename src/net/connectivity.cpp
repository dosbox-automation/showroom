// This file is part of the dosbox-automation-showroom Project.
// License: GPL-3.0-or-later. Contact: dosbox-automation-showroom-project@trinity2k.net
//

#include "net/connectivity.h"

#include "app/logging.h"

#include <QNetworkInformation>

namespace showroom {
namespace {

constexpr const char* kLogComponent = "connectivity";

bool reachabilityMeansOnline(QNetworkInformation::Reachability r)
{
    return r == QNetworkInformation::Reachability::Online
        || r == QNetworkInformation::Reachability::Site;
}

} // namespace

Connectivity::Connectivity(QObject* parent)
        : QObject(parent)
{
    backend_loaded_ = QNetworkInformation::loadDefaultBackend();
    if (!backend_loaded_) {
        log_warn(kLogComponent,
                 "no network information backend available, assuming online");
        return;
    }

    auto* info = QNetworkInformation::instance();
    log_info(kLogComponent,
             "using %s backend, reachability: %d",
             info->backendName().toUtf8().constData(),
             static_cast<int>(info->reachability()));

    connect(info,
            &QNetworkInformation::reachabilityChanged,
            this,
            [this](QNetworkInformation::Reachability r) {
                const bool online = reachabilityMeansOnline(r);
                log_info(kLogComponent,
                         "reachability changed to %d (%s)",
                         static_cast<int>(r),
                         online ? "online" : "offline");
                emit onlineChanged(online);
            });
}

bool Connectivity::isOnline() const
{
    if (!backend_loaded_) {
        return true;
    }
    auto* info = QNetworkInformation::instance();
    if (info == nullptr) {
        return true;
    }
    return reachabilityMeansOnline(info->reachability());
}

} // namespace showroom
