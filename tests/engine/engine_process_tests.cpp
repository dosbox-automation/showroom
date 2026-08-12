// This file is part of the dosbox-automation-showroom Project.
// License: GPL-3.0-or-later. Contact: dosbox-automation-showroom-project@trinity2k.net
//

#include "engine/engine_process.h"

#include <gtest/gtest.h>

#include <QCoreApplication>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QSignalSpy>
#include <QTemporaryDir>

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>

namespace showroom {
namespace {

bool isLowercaseHex(const std::string& text)
{
    return std::all_of(text.begin(), text.end(), [](char c) {
        const auto uc = static_cast<unsigned char>(c);
        return (std::isdigit(uc) != 0) || (c >= 'a' && c <= 'f');
    });
}

TEST(EngineProcessToken, issues_64_lowercase_hex_chars_and_never_repeats)
{
    const auto first = EngineProcess::generateApiToken();
    const auto second = EngineProcess::generateApiToken();

    EXPECT_EQ(first.size(), 64u);
    EXPECT_TRUE(isLowercaseHex(first));
    EXPECT_EQ(second.size(), 64u);
    EXPECT_TRUE(isLowercaseHex(second));
    EXPECT_NE(first, second);
}

// The child is the real fake_engine binary, so every test below exercises
// an actual spawn, real signals, and a real process teardown.
class EngineProcessFixture : public ::testing::Test {
protected:
    void SetUp() override
    {
        conf_path_ = base() / "run.conf";
        std::ofstream(conf_path_) << "[sdl]\n";
        report_path_ = base() / "report.txt";
        qputenv("FAKE_ENGINE_REPORT", QByteArray(report_path_.string().c_str()));
    }

    void TearDown() override
    {
        qunsetenv("FAKE_ENGINE_REPORT");
        qunsetenv("FAKE_ENGINE_MODE");
        qunsetenv("FAKE_ENGINE_SHUTDOWN_FILE");
        qunsetenv("FAKE_ENGINE_READY_FILE");
    }

    std::filesystem::path base() const
    {
        return std::filesystem::path(dir_.path().toStdString());
    }

    static std::filesystem::path fakeEngine()
    {
        return std::filesystem::path(FAKE_ENGINE_PATH);
    }

    static bool waitForFile(const std::filesystem::path& path, int timeout_ms)
    {
        QElapsedTimer elapsed;
        elapsed.start();
        while (!std::filesystem::exists(path)) {
            if (elapsed.elapsed() > timeout_ms) {
                return false;
            }
            QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
        }
        return true;
    }

    std::vector<std::string> reportLines() const
    {
        std::vector<std::string> lines;
        std::ifstream report(report_path_);
        std::string line;
        while (std::getline(report, line)) {
            lines.push_back(line);
        }
        return lines;
    }

    QTemporaryDir dir_;
    std::filesystem::path conf_path_;
    std::filesystem::path report_path_;
};

TEST_F(EngineProcessFixture, start_refuses_a_missing_binary)
{
    EngineProcess process(base() / "no-such-engine");
    std::string error;

    EXPECT_FALSE(process.start(conf_path_, error));
    EXPECT_FALSE(error.empty());
    EXPECT_FALSE(process.isRunning());
}

TEST_F(EngineProcessFixture, start_refuses_a_missing_conf)
{
    EngineProcess process(fakeEngine());
    std::string error;

    EXPECT_FALSE(process.start(base() / "no-such.conf", error));
    EXPECT_FALSE(error.empty());
    EXPECT_FALSE(process.isRunning());
}

TEST_F(EngineProcessFixture, start_refuses_a_binary_without_exec_permission)
{
    const auto plain_file = base() / "not-executable";
    std::ofstream(plain_file) << "data";

    EngineProcess process(plain_file);
    std::string error;

    EXPECT_FALSE(process.start(conf_path_, error));
    EXPECT_FALSE(error.empty());
}

TEST_F(EngineProcessFixture, child_receives_token_in_env_and_conf_in_arguments)
{
    EngineProcess process(fakeEngine());
    QSignalSpy finished(&process, &EngineProcess::finished);
    std::string error;

    ASSERT_TRUE(process.start(conf_path_, error)) << error;
    ASSERT_TRUE(finished.wait(5000));

    const auto lines = reportLines();
    ASSERT_EQ(lines.size(), 5u);
    EXPECT_EQ(lines[0], "token=" + process.token());
    EXPECT_EQ(process.token().size(), 64u);
    EXPECT_TRUE(isLowercaseHex(process.token()));
    EXPECT_EQ(lines[1], "arg=-noprimaryconf");
    EXPECT_EQ(lines[2], "arg=-nolocalconf");
    EXPECT_EQ(lines[3], "arg=-conf");
    EXPECT_EQ(lines[4], "arg=" + conf_path_.string());
}

TEST_F(EngineProcessFixture, started_fires_and_finished_reports_the_exit_code)
{
    EngineProcess process(fakeEngine());
    QSignalSpy started(&process, &EngineProcess::started);
    QSignalSpy finished(&process, &EngineProcess::finished);
    std::string error;

    ASSERT_TRUE(process.start(conf_path_, error)) << error;
    ASSERT_TRUE(finished.wait(5000));

    EXPECT_EQ(started.count(), 1);
    ASSERT_EQ(finished.count(), 1);
    EXPECT_EQ(finished.at(0).at(0).toInt(), 7);
    EXPECT_FALSE(process.isRunning());
}

TEST_F(EngineProcessFixture, each_start_issues_a_fresh_token)
{
    EngineProcess process(fakeEngine());
    std::string error;

    QSignalSpy finished(&process, &EngineProcess::finished);
    ASSERT_TRUE(process.start(conf_path_, error)) << error;
    ASSERT_TRUE(finished.wait(5000));
    const auto first = process.token();

    ASSERT_TRUE(process.start(conf_path_, error)) << error;
    ASSERT_TRUE(finished.wait(5000));

    EXPECT_NE(first, process.token());
}

TEST_F(EngineProcessFixture, start_refuses_while_the_child_runs)
{
    qputenv("FAKE_ENGINE_MODE", QByteArray("run"));
    EngineProcess process(fakeEngine());
    QSignalSpy started(&process, &EngineProcess::started);
    QSignalSpy finished(&process, &EngineProcess::finished);
    std::string error;

    ASSERT_TRUE(process.start(conf_path_, error)) << error;
    ASSERT_TRUE(started.wait(5000));
    EXPECT_TRUE(process.isRunning());

    EXPECT_FALSE(process.start(conf_path_, error));
    EXPECT_FALSE(error.empty());

    process.setStopTimeouts(100, 100);
    process.stop();
    ASSERT_TRUE(finished.wait(5000));
}

TEST_F(EngineProcessFixture, failed_fires_when_the_binary_cannot_execute)
{
    const auto garbage = base() / "garbage-binary";
    std::ofstream(garbage) << "\x7f not a real executable";
    std::filesystem::permissions(garbage,
                                 std::filesystem::perms::owner_all,
                                 std::filesystem::perm_options::replace);

    EngineProcess process(garbage);
    QSignalSpy started(&process, &EngineProcess::started);
    QSignalSpy failed(&process, &EngineProcess::failed);
    std::string error;

    ASSERT_TRUE(process.start(conf_path_, error)) << error;
    ASSERT_TRUE(failed.wait(5000));

    EXPECT_EQ(started.count(), 0);
    EXPECT_FALSE(failed.at(0).at(0).toString().isEmpty());
    EXPECT_FALSE(process.isRunning());
}

TEST_F(EngineProcessFixture, graceful_stop_prefers_the_shutdown_requester)
{
    qputenv("FAKE_ENGINE_MODE", QByteArray("run"));
    const auto shutdown_file = base() / "shutdown-request";
    qputenv("FAKE_ENGINE_SHUTDOWN_FILE", QByteArray(shutdown_file.string().c_str()));

    EngineProcess process(fakeEngine());
    QSignalSpy started(&process, &EngineProcess::started);
    QSignalSpy finished(&process, &EngineProcess::finished);
    std::string error;

    ASSERT_TRUE(process.start(conf_path_, error)) << error;
    ASSERT_TRUE(started.wait(5000));

    int requester_calls = 0;
    process.setShutdownRequester([&]() {
        ++requester_calls;
        std::ofstream(shutdown_file) << "bye";
        return true;
    });
    process.stop();
    ASSERT_TRUE(finished.wait(5000));

    EXPECT_EQ(requester_calls, 1);
    // Exit code 0 proves the child left through its own shutdown path,
    // not through terminate or kill.
    EXPECT_EQ(finished.at(0).at(0).toInt(), 0);
}

TEST_F(EngineProcessFixture, stop_terminates_when_the_requester_cannot_deliver)
{
    qputenv("FAKE_ENGINE_MODE", QByteArray("run"));
    EngineProcess process(fakeEngine());
    QSignalSpy started(&process, &EngineProcess::started);
    QSignalSpy finished(&process, &EngineProcess::finished);
    std::string error;

    ASSERT_TRUE(process.start(conf_path_, error)) << error;
    ASSERT_TRUE(started.wait(5000));

    process.setStopTimeouts(100, 5000);
    process.setShutdownRequester([]() { return false; });
    process.stop();
    ASSERT_TRUE(finished.wait(5000));

    EXPECT_EQ(finished.at(0).at(0).toInt(), -1);
}

TEST_F(EngineProcessFixture, stop_terminates_when_the_child_ignores_the_request)
{
    qputenv("FAKE_ENGINE_MODE", QByteArray("run"));
    EngineProcess process(fakeEngine());
    QSignalSpy started(&process, &EngineProcess::started);
    QSignalSpy finished(&process, &EngineProcess::finished);
    std::string error;

    ASSERT_TRUE(process.start(conf_path_, error)) << error;
    ASSERT_TRUE(started.wait(5000));

    process.setStopTimeouts(100, 5000);
    process.setShutdownRequester([]() { return true; });
    process.stop();
    ASSERT_TRUE(finished.wait(5000));

    EXPECT_EQ(finished.at(0).at(0).toInt(), -1);
}

TEST_F(EngineProcessFixture, stop_without_a_requester_goes_straight_to_terminate)
{
    qputenv("FAKE_ENGINE_MODE", QByteArray("run"));
    EngineProcess process(fakeEngine());
    QSignalSpy started(&process, &EngineProcess::started);
    QSignalSpy finished(&process, &EngineProcess::finished);
    std::string error;

    ASSERT_TRUE(process.start(conf_path_, error)) << error;
    ASSERT_TRUE(started.wait(5000));

    process.setStopTimeouts(100, 5000);
    process.stop();
    ASSERT_TRUE(finished.wait(5000));

    EXPECT_EQ(finished.at(0).at(0).toInt(), -1);
}

#ifndef _WIN32
TEST_F(EngineProcessFixture, stop_kills_a_child_that_ignores_terminate)
{
    qputenv("FAKE_ENGINE_MODE", QByteArray("stubborn"));
    // Terminating before the child installs SIG_IGN would pass this test
    // through the terminate path; the ready file closes that race.
    const auto ready_file = base() / "child-ready";
    qputenv("FAKE_ENGINE_READY_FILE", QByteArray(ready_file.string().c_str()));

    EngineProcess process(fakeEngine());
    QSignalSpy started(&process, &EngineProcess::started);
    QSignalSpy finished(&process, &EngineProcess::finished);
    std::string error;

    ASSERT_TRUE(process.start(conf_path_, error)) << error;
    ASSERT_TRUE(started.wait(5000));
    ASSERT_TRUE(waitForFile(ready_file, 5000));

    process.setStopTimeouts(100, 100);
    process.stop();
    ASSERT_TRUE(finished.wait(5000));

    EXPECT_EQ(finished.at(0).at(0).toInt(), -1);
    EXPECT_FALSE(process.isRunning());
}
#endif

} // namespace
} // namespace showroom
