// This file is part of the dosbox-automation-showroom Project.
// License: GPL-3.0-or-later. Contact: dosbox-automation-showroom-project@trinity2k.net
//

#ifndef SHOWROOM_NET_CONNECTIVITY_H
#define SHOWROOM_NET_CONNECTIVITY_H

#include <QObject>

namespace showroom {

// Reports whether the machine can reach the internet, without pinging
// anyone. Backed by QNetworkInformation where a platform backend is
// available; falls back to "assume online" otherwise, so a missing
// backend never blocks downloads.
class Connectivity : public QObject {
    Q_OBJECT

public:
    explicit Connectivity(QObject* parent = nullptr);

    virtual bool isOnline() const;

signals:
    void onlineChanged(bool online);

private:
    bool backend_loaded_ = false;
};

} // namespace showroom

#endif // SHOWROOM_NET_CONNECTIVITY_H
