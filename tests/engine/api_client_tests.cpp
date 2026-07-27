// This file is part of the dosbox-automation-showroom Project.
// License: GPL-3.0-or-later. Contact: dosbox-automation-showroom-project@trinity2k.net
//

#include "engine/api_client.h"

#include <gtest/gtest.h>

#include <QCoreApplication>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QHostAddress>
#include <QJsonObject>
#include <QTcpServer>
#include <QTcpSocket>

#include <optional>
#include <string>

namespace showroom {
namespace {

// Speaks just enough HTTP/1.1 to answer one canned response per
// connection, so the client tests never depend on a running engine.
class TestHttpServer : public QObject {
public:
    TestHttpServer()
    {
        connect(&server_, &QTcpServer::newConnection, this, [this]() {
            auto* socket = server_.nextPendingConnection();
            connect(socket, &QTcpSocket::readyRead, this, [this, socket]() {
                request_data += socket->readAll();
                if (!requestComplete() || silent) {
                    return;
                }
                socket->write(response_);
                socket->flush();
                socket->disconnectFromHost();
            });
        });
        server_.listen(QHostAddress::LocalHost, 0);
        respondWith(200, "{}");
    }

    quint16 port() const { return server_.serverPort(); }

    void respondWith(int status, const QByteArray& body)
    {
        response_ = "HTTP/1.1 " + QByteArray::number(status) + " Status\r\n"
                  + "Content-Type: application/json\r\n"
                  + "Content-Length: " + QByteArray::number(body.size()) + "\r\n"
                  + "Connection: close\r\n\r\n" + body;
    }

    QByteArray requestLine() const
    {
        return request_data.left(request_data.indexOf("\r\n"));
    }

    QByteArray headerValue(const QByteArray& name) const
    {
        for (const auto& line : request_data.split('\n')) {
            if (line.toLower().startsWith(name.toLower() + ":")) {
                return line.mid(name.size() + 1).trimmed();
            }
        }
        return {};
    }

    QByteArray body() const
    {
        const auto split = request_data.indexOf("\r\n\r\n");
        return split < 0 ? QByteArray() : request_data.mid(split + 4);
    }

    QByteArray request_data;
    bool silent = false;

private:
    bool requestComplete() const
    {
        const auto header_end = request_data.indexOf("\r\n\r\n");
        if (header_end < 0) {
            return false;
        }
        const auto length_text = headerValue("Content-Length");
        const auto expected = length_text.isEmpty() ? 0 : length_text.toInt();
        return request_data.size() >= header_end + 4 + expected;
    }

    QTcpServer server_;
    QByteArray response_;
};

class ApiClientFixture : public ::testing::Test {
protected:
    // The handler runs on the event loop, so every call is pumped with a
    // ceiling rather than waited on unbounded.
    static std::optional<ApiClient::Response> await(
            const std::function<void(ApiClient::Handler)>& operation,
            int timeout_ms = 5000)
    {
        std::optional<ApiClient::Response> result;
        operation([&result](const ApiClient::Response& response) { result = response; });
        QElapsedTimer elapsed;
        elapsed.start();
        while (!result.has_value() && elapsed.elapsed() < timeout_ms) {
            QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
        }
        return result;
    }

    static constexpr const char* kToken =
            "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef";

    TestHttpServer server_;
};

TEST_F(ApiClientFixture, status_request_carries_method_path_and_bearer_token)
{
    server_.respondWith(200, R"({"running":true,"is_booted":false})");
    ApiClient api("127.0.0.1", server_.port(), kToken);

    const auto response = await([&](ApiClient::Handler h) { api.getStatus(h); });

    ASSERT_TRUE(response.has_value());
    EXPECT_TRUE(response->ok);
    EXPECT_EQ(response->http_status, 200);
    EXPECT_TRUE(response->body.value("running").toBool());
    EXPECT_EQ(server_.requestLine(), "GET /api/v1/status HTTP/1.1");
    EXPECT_EQ(server_.headerValue("Authorization"), QByteArray("Bearer ") + kToken);
}

TEST_F(ApiClientFixture, shutdown_posts_to_its_route)
{
    server_.respondWith(200, R"({"status":"shutdown_requested"})");
    ApiClient api("127.0.0.1", server_.port(), kToken);

    const auto response = await([&](ApiClient::Handler h) { api.requestShutdown(h); });

    ASSERT_TRUE(response.has_value());
    EXPECT_TRUE(response->ok);
    EXPECT_EQ(server_.requestLine(), "POST /api/v1/dosbox/shutdown HTTP/1.1");
}

TEST_F(ApiClientFixture, load_script_sends_the_body_with_name_and_content_type)
{
    server_.respondWith(200, R"({"status":"loaded","name":"doom"})");
    ApiClient api("127.0.0.1", server_.port(), kToken);
    const QByteArray script = "dosbox.log('hello')\n";

    const auto response = await(
            [&](ApiClient::Handler h) { api.loadScript("doom", script, h); });

    ASSERT_TRUE(response.has_value());
    EXPECT_TRUE(response->ok);
    EXPECT_EQ(server_.requestLine(), "POST /api/v1/script/load?name=doom HTTP/1.1");
    EXPECT_EQ(server_.headerValue("Content-Type"), "text/plain");
    EXPECT_EQ(server_.body(), script);
}

TEST_F(ApiClientFixture, load_script_url_encodes_the_name)
{
    ApiClient api("127.0.0.1", server_.port(), kToken);

    const auto response = await(
            [&](ApiClient::Handler h) { api.loadScript("my game&x=1", "script", h); });

    ASSERT_TRUE(response.has_value());
    const auto line = server_.requestLine();
    EXPECT_TRUE(line.contains("name=my%20game%26x%3D1")) << line.toStdString();
}

TEST_F(ApiClientFixture, load_script_refuses_an_empty_name_without_a_request)
{
    ApiClient api("127.0.0.1", server_.port(), kToken);

    const auto response = await(
            [&](ApiClient::Handler h) { api.loadScript("", "script", h); });

    ASSERT_TRUE(response.has_value());
    EXPECT_FALSE(response->ok);
    EXPECT_FALSE(response->error.isEmpty());
    EXPECT_TRUE(server_.request_data.isEmpty());
}

TEST_F(ApiClientFixture, load_script_refuses_an_empty_script_without_a_request)
{
    ApiClient api("127.0.0.1", server_.port(), kToken);

    const auto response = await(
            [&](ApiClient::Handler h) { api.loadScript("doom", "", h); });

    ASSERT_TRUE(response.has_value());
    EXPECT_FALSE(response->ok);
    EXPECT_FALSE(response->error.isEmpty());
    EXPECT_TRUE(server_.request_data.isEmpty());
}

TEST_F(ApiClientFixture, start_script_posts_to_its_route)
{
    server_.respondWith(200, R"({"status":"started"})");
    ApiClient api("127.0.0.1", server_.port(), kToken);

    const auto response = await([&](ApiClient::Handler h) { api.startScript(h); });

    ASSERT_TRUE(response.has_value());
    EXPECT_TRUE(response->ok);
    EXPECT_EQ(server_.requestLine(), "POST /api/v1/script/start HTTP/1.1");
}

TEST_F(ApiClientFixture, script_status_delivers_state_frame_and_output)
{
    server_.respondWith(
            200,
            R"({"state":"yielded","frame":123,"name":"doom","output":{"progress":55}})");
    ApiClient api("127.0.0.1", server_.port(), kToken);

    const auto response = await([&](ApiClient::Handler h) { api.getScriptStatus(h); });

    ASSERT_TRUE(response.has_value());
    EXPECT_TRUE(response->ok);
    EXPECT_EQ(server_.requestLine(), "GET /api/v1/script/status HTTP/1.1");
    EXPECT_EQ(response->body.value("state").toString(), "yielded");
    EXPECT_EQ(response->body.value("frame").toInt(), 123);
    EXPECT_EQ(response->body.value("output").toObject().value("progress").toInt(), 55);
}

TEST_F(ApiClientFixture, an_http_error_is_a_typed_failure)
{
    server_.respondWith(400, R"({"error":"no script loaded"})");
    ApiClient api("127.0.0.1", server_.port(), kToken);

    const auto response = await([&](ApiClient::Handler h) { api.startScript(h); });

    ASSERT_TRUE(response.has_value());
    EXPECT_FALSE(response->ok);
    EXPECT_EQ(response->http_status, 400);
    EXPECT_TRUE(response->error.contains("no script loaded"))
            << response->error.toStdString();
}

TEST_F(ApiClientFixture, an_unreachable_server_is_a_typed_failure)
{
    // An ephemeral listener closed before use leaves a port nothing owns.
    quint16 dead_port = 0;
    {
        QTcpServer probe;
        probe.listen(QHostAddress::LocalHost, 0);
        dead_port = probe.serverPort();
    }
    ApiClient api("127.0.0.1", dead_port, kToken);

    const auto response = await([&](ApiClient::Handler h) { api.getStatus(h); });

    ASSERT_TRUE(response.has_value());
    EXPECT_FALSE(response->ok);
    EXPECT_EQ(response->http_status, 0);
    EXPECT_FALSE(response->error.isEmpty());
}

TEST_F(ApiClientFixture, a_stalled_server_reports_a_timeout)
{
    server_.silent = true;
    ApiClient api("127.0.0.1", server_.port(), kToken);
    api.setRequestTimeout(200);

    const auto response = await([&](ApiClient::Handler h) { api.getStatus(h); });

    ASSERT_TRUE(response.has_value());
    EXPECT_FALSE(response->ok);
    EXPECT_EQ(response->http_status, 0);
    EXPECT_FALSE(response->error.isEmpty());
}

TEST_F(ApiClientFixture, a_garbage_body_on_success_is_a_typed_failure)
{
    server_.respondWith(200, "this is not json");
    ApiClient api("127.0.0.1", server_.port(), kToken);

    const auto response = await([&](ApiClient::Handler h) { api.getStatus(h); });

    ASSERT_TRUE(response.has_value());
    EXPECT_FALSE(response->ok);
    EXPECT_EQ(response->http_status, 200);
    EXPECT_FALSE(response->error.isEmpty());
}

} // namespace
} // namespace showroom
