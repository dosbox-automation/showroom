// This file is part of the dosbox-automation-showroom Project.
// License: GPL-3.0-or-later. Contact: dosbox-automation-showroom-project@trinity2k.net
//

#include "engine/api_client.h"

#include <QJsonDocument>
#include <QJsonParseError>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QUrl>

#include <utility>

namespace showroom {
namespace {

ApiClient::Response parseReply(QNetworkReply& reply)
{
    ApiClient::Response response;
    const auto status_attribute = reply.attribute(
            QNetworkRequest::HttpStatusCodeAttribute);
    response.http_status = status_attribute.isValid() ? status_attribute.toInt() : 0;

    QJsonParseError parse_error = {};
    const auto document = QJsonDocument::fromJson(reply.readAll(), &parse_error);
    const bool parsed = parse_error.error == QJsonParseError::NoError
                     && document.isObject();
    if (parsed) {
        response.body = document.object();
    }

    if (response.http_status == 0) {
        response.error = reply.errorString();
        return response;
    }
    if (response.http_status < 200 || response.http_status >= 300) {
        // The engine puts the reason into an "error" field; surface it in
        // preference to Qt's generic protocol message.
        const auto server_error = response.body.value("error").toString();
        response.error = server_error.isEmpty()
                               ? QString("HTTP %1").arg(response.http_status)
                               : server_error;
        return response;
    }
    if (!parsed) {
        response.error = "response body is not a JSON object";
        return response;
    }
    response.ok = true;
    return response;
}

} // namespace

ApiClient::ApiClient(QString host, quint16 port, std::string token, QObject* parent)
        : QObject(parent),
          host_(std::move(host)),
          port_(port),
          token_(std::move(token))
{}

void ApiClient::setRequestTimeout(int timeout_ms)
{
    timeout_ms_ = timeout_ms;
}

void ApiClient::sendRequest(const QByteArray& verb, const QString& path,
                            const QString& raw_query, const QByteArray& body,
                            const QByteArray& content_type, Handler handler)
{
    QUrl url;
    url.setScheme("http");
    url.setHost(host_);
    url.setPort(port_);
    url.setPath(path);
    if (!raw_query.isEmpty()) {
        url.setQuery(raw_query);
    }

    QNetworkRequest request(url);
    request.setRawHeader("Authorization", "Bearer " + QByteArray::fromStdString(token_));
    request.setTransferTimeout(timeout_ms_);
    if (!content_type.isEmpty()) {
        request.setHeader(QNetworkRequest::ContentTypeHeader, content_type);
    }

    QNetworkReply* reply = verb == "GET" ? network_.get(request)
                                         : network_.post(request, body);
    connect(reply,
            &QNetworkReply::finished,
            this,
            [reply, handler = std::move(handler)]() {
                handler(parseReply(*reply));
                reply->deleteLater();
            });
}

void ApiClient::reportFailure(const QString& reason, Handler handler)
{
    // Queued so the handler never runs inside the caller's own call.
    QMetaObject::invokeMethod(
            this,
            [reason, handler = std::move(handler)]() {
                Response response;
                response.error = reason;
                handler(response);
            },
            Qt::QueuedConnection);
}

void ApiClient::getStatus(Handler handler)
{
    sendRequest("GET", "/api/v1/status", {}, {}, {}, std::move(handler));
}

void ApiClient::requestShutdown(Handler handler)
{
    sendRequest("POST",
                "/api/v1/dosbox/shutdown",
                {},
                {},
                "application/json",
                std::move(handler));
}

void ApiClient::loadScript(const QString& name, const QByteArray& script, Handler handler)
{
    if (name.isEmpty()) {
        reportFailure("script name must not be empty", std::move(handler));
        return;
    }
    if (script.isEmpty()) {
        reportFailure("script must not be empty", std::move(handler));
        return;
    }
    const QString query = "name=" + QString::fromUtf8(QUrl::toPercentEncoding(name));
    sendRequest("POST",
                "/api/v1/script/load",
                query,
                script,
                "text/plain",
                std::move(handler));
}

void ApiClient::startScript(Handler handler)
{
    sendRequest("POST",
                "/api/v1/script/start",
                {},
                {},
                "application/json",
                std::move(handler));
}

void ApiClient::getScriptStatus(Handler handler)
{
    sendRequest("GET", "/api/v1/script/status", {}, {}, {}, std::move(handler));
}

} // namespace showroom
