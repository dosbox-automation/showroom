// This file is part of the dosbox-automation-showroom Project.
// License: GPL-3.0-or-later. Contact: dosbox-automation-showroom-project@trinity2k.net
//

#include "net/downloader.h"

#include <gtest/gtest.h>

#include <QCoreApplication>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QHostAddress>
#include <QTcpServer>
#include <QTcpSocket>
#include <QUrl>

#include <fstream>
#include <cstdint>
#include <functional>
#include <limits>
#include <string>

namespace showroom {
namespace {

constexpr const char* kPayload = "0123456789abcdefghij";
constexpr const char* kLastModified = "Thu, 01 Sep 1994 12:00:00 GMT";

// One canned answer per connection; the responder sees the full request
// so range tests can assert what the client actually sent.
class DownloadTestServer : public QObject {
public:
    using Responder = std::function<QByteArray(const QByteArray& request)>;

    DownloadTestServer()
    {
        connect(&server_, &QTcpServer::newConnection, this, [this]() {
            auto* socket = server_.nextPendingConnection();
            connect(socket, &QTcpSocket::readyRead, this, [this, socket]() {
                last_request += socket->readAll();
                if (!last_request.contains("\r\n\r\n")) {
                    return;
                }
                socket->write(responder(last_request));
                socket->flush();
                if (!keep_open) {
                    socket->disconnectFromHost();
                }
            });
        });
        server_.listen(QHostAddress::LocalHost, 0);
        responder = [](const QByteArray&) { return fullResponse(kPayload); };
    }

    QUrl url() const
    {
        return QUrl(QStringLiteral("http://127.0.0.1:%1/archive.zip")
                            .arg(server_.serverPort()));
    }

    static QByteArray fullResponse(const QByteArray& body,
                                   const QByteArray& last_modified = kLastModified)
    {
        QByteArray head = "HTTP/1.1 200 OK\r\nContent-Length: "
                        + QByteArray::number(body.size()) + "\r\n";
        if (!last_modified.isEmpty()) {
            head += "Last-Modified: " + last_modified + "\r\n";
        }
        return head + "Connection: close\r\n\r\n" + body;
    }

    static QByteArray partialResponse(const QByteArray& full_body, qint64 from)
    {
        const QByteArray tail = full_body.mid(from);
        return "HTTP/1.1 206 Partial Content\r\nContent-Length: "
             + QByteArray::number(tail.size()) + "\r\nContent-Range: bytes "
             + QByteArray::number(from) + "-" + QByteArray::number(full_body.size() - 1)
             + "/" + QByteArray::number(full_body.size())
             + "\r\nConnection: close\r\n\r\n" + tail;
    }

    static QByteArray notFoundResponse()
    {
        return "HTTP/1.1 404 Not Found\r\nContent-Length: 9\r\n"
               "Connection: close\r\n\r\nnot found";
    }

    // Claims more bytes than it sends, then closes.
    static QByteArray truncatedResponse(const QByteArray& body, int missing_bytes)
    {
        return "HTTP/1.1 200 OK\r\nContent-Length: "
             + QByteArray::number(body.size() + missing_bytes)
             + "\r\nConnection: close\r\n\r\n" + body;
    }

    Responder responder;
    QByteArray last_request;
    bool keep_open = false;

private:
    QTcpServer server_;
};

class DownloaderFixture : public testing::Test {
protected:
    void SetUp() override
    {
        const auto* info = testing::UnitTest::GetInstance()->current_test_info();
        dir_ = std::filesystem::temp_directory_path()
             / ("showroom-download-"
                + std::to_string(testing::UnitTest::GetInstance()->random_seed()) + "-"
                + info->name());
        std::filesystem::remove_all(dir_);
        std::filesystem::create_directories(dir_);
        std::filesystem::permissions(dir_,
                                     std::filesystem::perms::owner_all,
                                     std::filesystem::perm_options::replace);
    }

    void TearDown() override
    {
        // An unwritable-directory test must not survive itself.
        std::filesystem::permissions(dir_,
                                     std::filesystem::perms::owner_all,
                                     std::filesystem::perm_options::replace);
        std::filesystem::remove_all(dir_);
    }

    static bool pumpUntil(const std::function<bool()>& done, int timeout_ms = 5000)
    {
        QElapsedTimer elapsed;
        elapsed.start();
        while (!done() && elapsed.elapsed() < timeout_ms) {
            QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
        }
        return done();
    }

    std::filesystem::path destination() const { return dir_ / "archive.zip"; }
    std::filesystem::path partFile() const { return dir_ / "archive.zip.part"; }

    // Progress signals are throttled and stop with the data, so a parked
    // transfer is observed through the bytes it flushed to disk.
    bool partHoldsAtLeast(std::uintmax_t byte_count) const
    {
        std::error_code ec;
        const auto size = std::filesystem::file_size(partFile(), ec);
        return !ec && size >= byte_count;
    }

    void writePart(const std::string& content)
    {
        std::ofstream out(partFile(), std::ios::binary);
        out << content;
    }

    static std::string readFile(const std::filesystem::path& path)
    {
        std::ifstream in(path, std::ios::binary);
        return std::string(std::istreambuf_iterator<char>(in),
                           std::istreambuf_iterator<char>());
    }

    DownloadTestServer server_;
    std::filesystem::path dir_;
};

struct Outcome {
    int finished = 0;
    int failed = 0;
    int cancelled = 0;
    QString fail_reason;
    qint64 last_received = -1;
    qint64 last_total = -1;
};

Outcome observe(Downloader& downloader)
{
    Outcome outcome;
    QObject::connect(&downloader, &Downloader::finished, [&outcome](const QString&) {
        ++outcome.finished;
    });
    QObject::connect(&downloader, &Downloader::failed, [&outcome](const QString& reason) {
        ++outcome.failed;
        outcome.fail_reason = reason;
    });
    QObject::connect(&downloader, &Downloader::cancelled, [&outcome]() {
        ++outcome.cancelled;
    });
    QObject::connect(&downloader,
                     &Downloader::progress,
                     [&outcome](qint64 received, qint64 total) {
                         outcome.last_received = received;
                         outcome.last_total = total;
                     });
    return outcome;
}

TEST_F(DownloaderFixture, a_complete_download_lands_at_the_final_name)
{
    Downloader downloader;
    Outcome outcome = observe(downloader);
    std::string error;

    ASSERT_TRUE(downloader.start(server_.url(), destination(), error)) << error;
    ASSERT_TRUE(pumpUntil([&] { return outcome.finished + outcome.failed > 0; }));

    EXPECT_EQ(outcome.finished, 1);
    EXPECT_EQ(readFile(destination()), kPayload);
    EXPECT_FALSE(std::filesystem::exists(partFile()));
    EXPECT_FALSE(downloader.isActive());
    // The completion emission is the one Qt never throttles away.
    EXPECT_EQ(outcome.last_received, 20);
    EXPECT_EQ(outcome.last_total, 20);
}

TEST_F(DownloaderFixture, the_servers_last_modified_becomes_the_files_mtime)
{
    Downloader downloader;
    Outcome outcome = observe(downloader);
    std::string error;

    ASSERT_TRUE(downloader.start(server_.url(), destination(), error)) << error;
    ASSERT_TRUE(pumpUntil([&] { return outcome.finished > 0; }));

    // 1994-09-01 12:00:00 UTC as the file clock sees it.
    const auto mtime = std::filesystem::last_write_time(destination());
    const auto expected = std::chrono::clock_cast<std::chrono::file_clock>(
            std::chrono::sys_seconds(std::chrono::seconds(778420800)));
    EXPECT_EQ(mtime, expected);
}

TEST_F(DownloaderFixture, a_404_leaves_no_files_behind)
{
    server_.responder = [](const QByteArray&) {
        return DownloadTestServer::notFoundResponse();
    };
    Downloader downloader;
    Outcome outcome = observe(downloader);
    std::string error;

    ASSERT_TRUE(downloader.start(server_.url(), destination(), error)) << error;
    ASSERT_TRUE(pumpUntil([&] { return outcome.failed > 0; }));

    EXPECT_NE(outcome.fail_reason.indexOf(QStringLiteral("404")), -1);
    EXPECT_FALSE(std::filesystem::exists(destination()));
    EXPECT_FALSE(std::filesystem::exists(partFile()));
    EXPECT_FALSE(downloader.isActive());
}

TEST_F(DownloaderFixture, a_truncated_body_keeps_the_part_and_reports_failure)
{
    server_.responder = [](const QByteArray&) {
        return DownloadTestServer::truncatedResponse("01234567", 12);
    };
    Downloader downloader;
    Outcome outcome = observe(downloader);
    std::string error;

    ASSERT_TRUE(downloader.start(server_.url(), destination(), error)) << error;
    ASSERT_TRUE(pumpUntil([&] { return outcome.failed > 0; }));

    EXPECT_FALSE(std::filesystem::exists(destination()));
    EXPECT_TRUE(std::filesystem::exists(partFile()));
    EXPECT_EQ(readFile(partFile()), "01234567");
}

TEST_F(DownloaderFixture, a_resume_sends_the_range_and_completes_the_file)
{
    writePart("01234567");
    server_.responder = [](const QByteArray&) {
        return DownloadTestServer::partialResponse(kPayload, 8);
    };
    Downloader downloader;
    Outcome outcome = observe(downloader);
    std::string error;

    ASSERT_TRUE(downloader.start(server_.url(), destination(), error)) << error;
    ASSERT_TRUE(pumpUntil([&] { return outcome.finished + outcome.failed > 0; }));

    EXPECT_EQ(outcome.finished, 1);
    EXPECT_NE(server_.last_request.toLower().indexOf("range: bytes=8-"), -1)
            << server_.last_request.constData();
    EXPECT_EQ(readFile(destination()), kPayload);
}

TEST_F(DownloaderFixture, a_server_ignoring_the_range_restarts_from_zero)
{
    // Stale garbage in the part file: if the full answer were appended
    // to it, the archive would verify against nothing.
    writePart("XXXXXXXX");
    Downloader downloader;
    Outcome outcome = observe(downloader);
    std::string error;

    ASSERT_TRUE(downloader.start(server_.url(), destination(), error)) << error;
    ASSERT_TRUE(pumpUntil([&] { return outcome.finished + outcome.failed > 0; }));

    EXPECT_EQ(outcome.finished, 1);
    EXPECT_EQ(readFile(destination()), kPayload);
}

TEST_F(DownloaderFixture, an_unwritable_destination_reports_instead_of_crashing)
{
    std::filesystem::permissions(dir_,
                                 std::filesystem::perms::owner_read
                                         | std::filesystem::perms::owner_exec,
                                 std::filesystem::perm_options::replace);
    Downloader downloader;
    Outcome outcome = observe(downloader);
    std::string error;

    const bool started = downloader.start(server_.url(), destination(), error);
    if (started) {
        ASSERT_TRUE(pumpUntil([&] { return outcome.failed > 0; }));
    } else {
        EXPECT_FALSE(error.empty());
    }
    EXPECT_FALSE(downloader.isActive());
}

TEST_F(DownloaderFixture, cancel_mid_transfer_keeps_the_part)
{
    server_.keep_open = true;
    server_.responder = [](const QByteArray&) {
        // Header promises more than arrives, and the socket stays open,
        // so the transfer parks mid-flight for the cancel.
        return DownloadTestServer::truncatedResponse("01234567", 100);
    };
    Downloader downloader;
    Outcome outcome = observe(downloader);
    std::string error;

    ASSERT_TRUE(downloader.start(server_.url(), destination(), error)) << error;
    ASSERT_TRUE(pumpUntil([&] { return partHoldsAtLeast(8); }));

    downloader.cancel();
    ASSERT_TRUE(pumpUntil([&] { return outcome.cancelled > 0; }));

    EXPECT_EQ(outcome.failed, 0);
    EXPECT_TRUE(std::filesystem::exists(partFile()));
    EXPECT_FALSE(std::filesystem::exists(destination()));
    EXPECT_FALSE(downloader.isActive());
}

TEST_F(DownloaderFixture, a_download_larger_than_the_free_disk_is_refused)
{
    Downloader downloader;
    Outcome outcome = observe(downloader);
    std::string error;

    const auto absurd = std::numeric_limits<std::uint64_t>::max() / 2;
    EXPECT_FALSE(downloader.start(server_.url(), destination(), error, absurd));

    EXPECT_NE(error.find("space"), std::string::npos) << error;
    EXPECT_TRUE(server_.last_request.isEmpty());
    EXPECT_FALSE(std::filesystem::exists(partFile()));
    EXPECT_FALSE(downloader.isActive());
    EXPECT_EQ(outcome.failed, 0);
}

TEST_F(DownloaderFixture, a_download_that_fits_passes_the_space_check)
{
    Downloader downloader;
    Outcome outcome = observe(downloader);
    std::string error;

    ASSERT_TRUE(downloader.start(server_.url(), destination(), error, 20)) << error;
    ASSERT_TRUE(pumpUntil([&] { return outcome.finished + outcome.failed > 0; }));

    EXPECT_EQ(outcome.finished, 1);
    EXPECT_EQ(readFile(destination()), kPayload);
}

TEST_F(DownloaderFixture, a_second_start_while_active_is_refused)
{
    server_.keep_open = true;
    server_.responder = [](const QByteArray&) {
        return DownloadTestServer::truncatedResponse("01234567", 100);
    };
    Downloader downloader;
    Outcome outcome = observe(downloader);
    std::string error;

    ASSERT_TRUE(downloader.start(server_.url(), destination(), error)) << error;
    ASSERT_TRUE(pumpUntil([&] { return partHoldsAtLeast(8); }));

    EXPECT_FALSE(downloader.start(server_.url(), dir_ / "other.zip", error));
    EXPECT_FALSE(error.empty());

    downloader.cancel();
    ASSERT_TRUE(pumpUntil([&] { return outcome.cancelled > 0; }));
}

} // namespace
} // namespace showroom
