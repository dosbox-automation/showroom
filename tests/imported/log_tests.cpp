// This file is part of the dosbox-automation-showroom Project.
// License: GPL-3.0-or-later. Contact: dosbox-automation-showroom-project@trinity2k.net
//
// Copied from the augra-engine Project (house sibling, GPL-3.0-or-later),
// tests/unit/test_log.cpp, with the namespace and header guard changed and nothing
// else. Kept as a copy rather than a shared library by Mother's call: two
// self-contained files with no dependencies beyond the standard library do
// not justify a separate repository, its own build, and two consumers
// pulling it. A fix that applies to both is a copy, not a release.

#include <gtest/gtest.h>
#include "imported/log.h"

#include <string>
#include <vector>

using namespace showroom;

namespace {

struct CapturedMessage {
    LogLevel level;
    std::string component;
    std::string message;
};

class LogTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        auto& logger = Logger::instance();
        logger.clear_sinks();
        logger.set_level(LogLevel::Trace);
        logger.add_sink([this](LogLevel level, const char* component,
                               const std::string& message) {
            captured_.push_back({level, component, message});
        });
    }

    void TearDown() override
    {
        // restore default state
        auto& logger = Logger::instance();
        logger.clear_sinks();
        logger.set_level(LogLevel::Info);
    }

    std::vector<CapturedMessage> captured_;
};

} // anonymous namespace

TEST_F(LogTest, InfoReachesCorrectSink)
{
    Logger::instance().info("engine", "started version %s", "0.1.0");

    ASSERT_EQ(captured_.size(), 1u);
    EXPECT_EQ(captured_[0].level, LogLevel::Info);
    EXPECT_EQ(captured_[0].component, "engine");
    EXPECT_EQ(captured_[0].message, "started version 0.1.0");
}

TEST_F(LogTest, LevelFiltering)
{
    Logger::instance().set_level(LogLevel::Warn);

    Logger::instance().trace("test", "trace");
    Logger::instance().debug("test", "debug");
    Logger::instance().info("test", "info");
    Logger::instance().warn("test", "warn");
    Logger::instance().error("test", "error");

    ASSERT_EQ(captured_.size(), 2u);
    EXPECT_EQ(captured_[0].level, LogLevel::Warn);
    EXPECT_EQ(captured_[1].level, LogLevel::Error);
}

TEST_F(LogTest, TracePassesAtTraceLevel)
{
    Logger::instance().set_level(LogLevel::Trace);
    Logger::instance().trace("test", "visible");

    ASSERT_EQ(captured_.size(), 1u);
    EXPECT_EQ(captured_[0].level, LogLevel::Trace);
}

TEST_F(LogTest, AllLevelsAtTrace)
{
    Logger::instance().set_level(LogLevel::Trace);

    Logger::instance().trace("t", "1");
    Logger::instance().debug("t", "2");
    Logger::instance().info("t", "3");
    Logger::instance().warn("t", "4");
    Logger::instance().error("t", "5");

    EXPECT_EQ(captured_.size(), 5u);
}

TEST_F(LogTest, ErrorLevelOnlyShowsError)
{
    Logger::instance().set_level(LogLevel::Error);

    Logger::instance().trace("t", "no");
    Logger::instance().debug("t", "no");
    Logger::instance().info("t", "no");
    Logger::instance().warn("t", "no");
    Logger::instance().error("t", "yes");

    ASSERT_EQ(captured_.size(), 1u);
    EXPECT_EQ(captured_[0].message, "yes");
}

TEST_F(LogTest, MultipleSinks)
{
    int second_count = 0;
    Logger::instance().add_sink([&](LogLevel, const char*, const std::string&) {
        second_count++;
    });

    Logger::instance().info("test", "hello");

    EXPECT_EQ(captured_.size(), 1u);
    EXPECT_EQ(second_count, 1);
}

TEST_F(LogTest, FormatWithIntegers)
{
    Logger::instance().info("config", "port=%d buffer=%d", 8386, 1024);

    ASSERT_EQ(captured_.size(), 1u);
    EXPECT_EQ(captured_[0].message, "port=8386 buffer=1024");
}

TEST_F(LogTest, FormatWithNoArgs)
{
    Logger::instance().info("engine", "no arguments");

    ASSERT_EQ(captured_.size(), 1u);
    EXPECT_EQ(captured_[0].message, "no arguments");
}

TEST_F(LogTest, ComponentPropagated)
{
    Logger::instance().info("backend", "window opened");
    Logger::instance().warn("platform", "card not found");

    ASSERT_EQ(captured_.size(), 2u);
    EXPECT_EQ(captured_[0].component, "backend");
    EXPECT_EQ(captured_[1].component, "platform");
}

TEST_F(LogTest, LevelNames)
{
    EXPECT_STREQ(log_level_name(LogLevel::Trace), "TRACE");
    EXPECT_STREQ(log_level_name(LogLevel::Debug), "DEBUG");
    EXPECT_STREQ(log_level_name(LogLevel::Info), "INFO");
    EXPECT_STREQ(log_level_name(LogLevel::Warn), "WARN");
    EXPECT_STREQ(log_level_name(LogLevel::Error), "ERROR");
}

TEST_F(LogTest, FreeFunctions)
{
    log_info("runner", "game=%s", "hack1");
    log_warn("audio", "buffer underrun");

    ASSERT_EQ(captured_.size(), 2u);
    EXPECT_EQ(captured_[0].component, "runner");
    EXPECT_EQ(captured_[0].message, "game=hack1");
    EXPECT_EQ(captured_[1].level, LogLevel::Warn);
}

TEST_F(LogTest, ClearSinksStopsOutput)
{
    Logger::instance().clear_sinks();
    Logger::instance().info("test", "silent");

    EXPECT_EQ(captured_.size(), 0u);
}

// -- Per-component level filtering --

TEST_F(LogTest, ComponentLevelOverridesGlobal)
{
    Logger::instance().set_level(LogLevel::Warn);
    Logger::instance().set_component_level("MDA_Card", LogLevel::Trace);

    log_trace("MDA_Card", "visible because component is at Trace");
    log_trace("platform", "invisible because global is at Warn");

    ASSERT_EQ(captured_.size(), 1u);
    EXPECT_EQ(captured_[0].component, "MDA_Card");

    Logger::instance().clear_all_component_levels();
}

TEST_F(LogTest, ComponentLevelCanBeMoreRestrictive)
{
    Logger::instance().set_level(LogLevel::Trace);
    Logger::instance().set_component_level("noisy", LogLevel::Error);

    log_info("noisy", "suppressed");
    log_error("noisy", "visible");
    log_info("other", "also visible");

    ASSERT_EQ(captured_.size(), 2u);
    EXPECT_EQ(captured_[0].component, "noisy");
    EXPECT_EQ(captured_[0].level, LogLevel::Error);
    EXPECT_EQ(captured_[1].component, "other");

    Logger::instance().clear_all_component_levels();
}

TEST_F(LogTest, ClearComponentLevelRevertsToGlobal)
{
    Logger::instance().set_level(LogLevel::Warn);
    Logger::instance().set_component_level("test", LogLevel::Trace);

    log_trace("test", "visible");
    ASSERT_EQ(captured_.size(), 1u);

    Logger::instance().clear_component_level("test");
    log_trace("test", "now invisible");
    EXPECT_EQ(captured_.size(), 1u);
}

TEST_F(LogTest, ClearAllComponentLevels)
{
    Logger::instance().set_level(LogLevel::Error);
    Logger::instance().set_component_level("a", LogLevel::Trace);
    Logger::instance().set_component_level("b", LogLevel::Trace);

    Logger::instance().clear_all_component_levels();

    log_info("a", "suppressed");
    log_info("b", "suppressed");
    EXPECT_EQ(captured_.size(), 0u);
}

TEST_F(LogTest, EffectiveLevelReturnsComponentOrGlobal)
{
    Logger::instance().set_level(LogLevel::Info);
    EXPECT_EQ(Logger::instance().effective_level("test"), LogLevel::Info);

    Logger::instance().set_component_level("test", LogLevel::Debug);
    EXPECT_EQ(Logger::instance().effective_level("test"), LogLevel::Debug);
    EXPECT_EQ(Logger::instance().effective_level("other"), LogLevel::Info);

    Logger::instance().clear_all_component_levels();
}

// -- Handler tests --

namespace {

class CaptureHandler : public LogHandler {
public:
    struct Entry {
        LogLevel level;
        std::string component;
        std::string message;
        std::string formatted;
    };

    void emit(LogLevel level, const char* component,
              const std::string& message) override
    {
        if (!accepts(level)) return;
        entries.push_back({level, component, message,
                           format_output(level, component, message)});
    }

    std::vector<Entry> entries;
};

class HandlerTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        auto& logger = Logger::instance();
        logger.clear_sinks();
        logger.set_level(LogLevel::Trace);
        logger.clear_all_component_levels();
        handler_ = std::make_shared<CaptureHandler>();
        logger.add_handler(handler_);
    }

    void TearDown() override
    {
        auto& logger = Logger::instance();
        logger.clear_sinks();
        logger.clear_handlers();
        logger.set_level(LogLevel::Info);
        logger.clear_all_component_levels();
    }

    std::shared_ptr<CaptureHandler> handler_;
};

} // anonymous namespace

TEST_F(HandlerTest, HandlerReceivesMessages)
{
    log_info("engine", "hello from handler");

    ASSERT_EQ(handler_->entries.size(), 1u);
    EXPECT_EQ(handler_->entries[0].component, "engine");
    EXPECT_EQ(handler_->entries[0].message, "hello from handler");
}

TEST_F(HandlerTest, HandlerLevelFiltering)
{
    handler_->set_level(LogLevel::Warn);

    log_info("test", "filtered by handler");
    log_warn("test", "passes handler");

    ASSERT_EQ(handler_->entries.size(), 1u);
    EXPECT_EQ(handler_->entries[0].level, LogLevel::Warn);
}

TEST_F(HandlerTest, MultipleHandlersIndependent)
{
    auto second = std::make_shared<CaptureHandler>();
    second->set_level(LogLevel::Error);
    Logger::instance().add_handler(second);

    log_warn("test", "first only");
    log_error("test", "both");

    EXPECT_EQ(handler_->entries.size(), 2u);
    EXPECT_EQ(second->entries.size(), 1u);
    EXPECT_EQ(second->entries[0].level, LogLevel::Error);
}

TEST_F(HandlerTest, RemoveHandler)
{
    Logger::instance().remove_handler(handler_);
    log_info("test", "nobody home");

    EXPECT_EQ(handler_->entries.size(), 0u);
}

TEST_F(HandlerTest, DefaultFormatString)
{
    log_info("engine", "started");

    ASSERT_EQ(handler_->entries.size(), 1u);
    EXPECT_EQ(handler_->entries[0].formatted, "[INFO] engine: started");
}

TEST_F(HandlerTest, CustomFormatString)
{
    handler_->set_format("{level} | {component} | {message}");
    log_warn("audio", "underrun");

    ASSERT_EQ(handler_->entries.size(), 1u);
    EXPECT_EQ(handler_->entries[0].formatted, "WARN | audio | underrun");
}

TEST_F(HandlerTest, FormatUnknownTokenPassedThrough)
{
    handler_->set_format("{level} {unknown} {message}");
    log_info("test", "hello");

    ASSERT_EQ(handler_->entries.size(), 1u);
    EXPECT_EQ(handler_->entries[0].formatted, "INFO {unknown} hello");
}

TEST_F(HandlerTest, FormatMinimal)
{
    handler_->set_format("{message}");
    log_info("test", "bare message");

    ASSERT_EQ(handler_->entries.size(), 1u);
    EXPECT_EQ(handler_->entries[0].formatted, "bare message");
}

// -- FileHandler --

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <sstream>

namespace {

class FileHandlerTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        auto& logger = Logger::instance();
        logger.clear_sinks();
        logger.clear_handlers();
        logger.set_level(LogLevel::Trace);
        logger.clear_all_component_levels();

        tmp_dir_ = std::filesystem::temp_directory_path() /
                   ("showroom_test_log_" + std::to_string(reinterpret_cast<uintptr_t>(this)));
        std::filesystem::create_directories(tmp_dir_);
        log_path_ = tmp_dir_ / "test.log";
    }

    void TearDown() override
    {
        auto& logger = Logger::instance();
        logger.clear_sinks();
        logger.clear_handlers();
        logger.set_level(LogLevel::Info);
        logger.clear_all_component_levels();

        std::filesystem::remove_all(tmp_dir_);
    }

    std::string read_log_file() const
    {
        std::ifstream f(log_path_);
        std::ostringstream ss;
        ss << f.rdbuf();
        return ss.str();
    }

    std::filesystem::path tmp_dir_;
    std::filesystem::path log_path_;
};

} // anonymous namespace

TEST_F(FileHandlerTest, WritesToFile)
{
    {
        auto fh = std::make_shared<FileHandler>(log_path_.string());
        ASSERT_TRUE(fh->is_open());
        Logger::instance().add_handler(fh);

        log_info("engine", "file test");
        Logger::instance().clear_handlers();
    }

    auto content = read_log_file();
    EXPECT_NE(content.find("[INFO] engine: file test"), std::string::npos);
}

TEST_F(FileHandlerTest, AppendsToExistingFile)
{
    {
        auto fh = std::make_shared<FileHandler>(log_path_.string());
        Logger::instance().add_handler(fh);
        log_info("test", "line one");
        Logger::instance().clear_handlers();
    }
    {
        auto fh = std::make_shared<FileHandler>(log_path_.string(), true);
        Logger::instance().add_handler(fh);
        log_info("test", "line two");
        Logger::instance().clear_handlers();
    }

    auto content = read_log_file();
    EXPECT_NE(content.find("line one"), std::string::npos);
    EXPECT_NE(content.find("line two"), std::string::npos);
}

TEST_F(FileHandlerTest, TruncateMode)
{
    {
        auto fh = std::make_shared<FileHandler>(log_path_.string());
        Logger::instance().add_handler(fh);
        log_info("test", "old content");
        Logger::instance().clear_handlers();
    }
    {
        auto fh = std::make_shared<FileHandler>(log_path_.string(), false);
        Logger::instance().add_handler(fh);
        log_info("test", "new content");
        Logger::instance().clear_handlers();
    }

    auto content = read_log_file();
    EXPECT_EQ(content.find("old content"), std::string::npos);
    EXPECT_NE(content.find("new content"), std::string::npos);
}

TEST_F(FileHandlerTest, HandlerLevelFilteringOnFile)
{
    auto fh = std::make_shared<FileHandler>(log_path_.string());
    fh->set_level(LogLevel::Error);
    Logger::instance().add_handler(fh);

    log_info("test", "filtered");
    log_error("test", "passes");
    Logger::instance().clear_handlers();

    auto content = read_log_file();
    EXPECT_EQ(content.find("filtered"), std::string::npos);
    EXPECT_NE(content.find("passes"), std::string::npos);
}

TEST_F(FileHandlerTest, CustomFormatOnFile)
{
    auto fh = std::make_shared<FileHandler>(log_path_.string());
    fh->set_format("{level}:{component}:{message}");
    Logger::instance().add_handler(fh);

    log_warn("net", "timeout");
    Logger::instance().clear_handlers();

    auto content = read_log_file();
    EXPECT_NE(content.find("WARN:net:timeout"), std::string::npos);
}

TEST_F(FileHandlerTest, InvalidPathDoesNotCrash)
{
    auto fh = std::make_shared<FileHandler>("/nonexistent/path/log.txt");
    EXPECT_FALSE(fh->is_open());

    Logger::instance().add_handler(fh);
    log_info("test", "silently dropped");
    Logger::instance().clear_handlers();
}

// -- Combined: component level + handler level --

TEST_F(HandlerTest, ComponentAndHandlerLevelsBothFilter)
{
    Logger::instance().set_level(LogLevel::Info);
    Logger::instance().set_component_level("verbose", LogLevel::Trace);
    handler_->set_level(LogLevel::Debug);

    log_trace("verbose", "passes component, blocked by handler");
    log_debug("verbose", "passes both");
    log_trace("other", "blocked by logger global");

    ASSERT_EQ(handler_->entries.size(), 1u);
    EXPECT_EQ(handler_->entries[0].message, "passes both");

    Logger::instance().clear_all_component_levels();
}
