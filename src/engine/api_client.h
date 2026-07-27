// This file is part of the dosbox-automation-showroom Project.
// License: GPL-3.0-or-later. Contact: dosbox-automation-showroom-project@trinity2k.net
//

#ifndef SHOWROOM_ENGINE_API_CLIENT_H
#define SHOWROOM_ENGINE_API_CLIENT_H

#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QObject>
#include <QString>

#include <functional>
#include <string>

namespace showroom {

// Speaks the engine's REST API with the launch token on every request.
// Every outcome arrives as a Response through the handler on the owning
// thread; nothing throws into the caller.
class ApiClient : public QObject {
    Q_OBJECT

public:
    struct Response {
        bool ok = false;
        // 0 means the request never got an HTTP answer at all.
        int http_status = 0;
        QJsonObject body;
        QString error;
    };
    using Handler = std::function<void(const Response&)>;

    ApiClient(QString host, quint16 port, std::string token, QObject* parent = nullptr);

    void setRequestTimeout(int timeout_ms);

    void getStatus(Handler handler);
    void requestShutdown(Handler handler);
    void loadScript(const QString& name, const QByteArray& script, Handler handler);
    void startScript(Handler handler);
    void getScriptStatus(Handler handler);

private:
    void sendRequest(const QByteArray& verb, const QString& path,
                     const QString& raw_query, const QByteArray& body,
                     const QByteArray& content_type, Handler handler);
    void reportFailure(const QString& reason, Handler handler);

    QString host_;
    quint16 port_ = 0;
    std::string token_;
    QNetworkAccessManager network_;
    // The script/status bridge may hold a reply up to 15 s server-side,
    // so the client ceiling sits above that.
    int timeout_ms_ = 20000;
};

} // namespace showroom

#endif // SHOWROOM_ENGINE_API_CLIENT_H
