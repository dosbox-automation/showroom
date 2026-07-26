// This file is part of the dosbox-automation-showroom Project.
// License: GPL-3.0-or-later. Contact: dosbox-automation-showroom-project@trinity2k.net
//
// Copied from the augra-engine Project (house sibling, GPL-3.0-or-later),
// augra/log.{h,cpp}, with the namespace and header guard changed and nothing
// else. Kept as a copy rather than a shared library by Mother's call: two
// self-contained files with no dependencies beyond the standard library do
// not justify a separate repository, its own build, and two consumers
// pulling it. A fix that applies to both is a copy, not a release.

// It keeps augra-engine's snake_case naming and its own formatting rather
// than this project's. That is why it lives under imported/: the linting
// scripts skip that directory, so the file stays close enough to its
// source that a fix in either tree crosses with a diff and a namespace
// change. Renaming twenty-three identifiers would buy local consistency
// and cost that property every time.

#include "imported/log.h"

#include <algorithm>
#include <cstdio>
#include <cstring>

namespace showroom {

const char* log_level_name(LogLevel level)
{
    switch (level) {
        case LogLevel::Trace: return "TRACE";
        case LogLevel::Debug: return "DEBUG";
        case LogLevel::Info:  return "INFO";
        case LogLevel::Warn:  return "WARN";
        case LogLevel::Error: return "ERROR";
    }
    return "?";
}

// -- LogHandler --

std::string LogHandler::format_output(LogLevel level, const char* component,
                                       const std::string& message) const
{
    std::string result;
    result.reserve(format_.size() + message.size() + 32);

    size_t pos = 0;
    while (pos < format_.size()) {
        size_t brace = format_.find('{', pos);
        if (brace == std::string::npos) {
            result.append(format_, pos);
            break;
        }

        result.append(format_, pos, brace - pos);

        size_t end = format_.find('}', brace);
        if (end == std::string::npos) {
            result.append(format_, brace);
            break;
        }

        auto token = format_.substr(brace + 1, end - brace - 1);
        if (token == "level") {
            result.append(log_level_name(level));
        } else if (token == "component") {
            result.append(component);
        } else if (token == "message") {
            result.append(message);
        } else {
            result.push_back('{');
            result.append(token);
            result.push_back('}');
        }

        pos = end + 1;
    }

    return result;
}

// -- StderrHandler --

void StderrHandler::emit(LogLevel level, const char* component,
                          const std::string& message)
{
    if (!accepts(level)) return;
    auto line = format_output(level, component, message);
    std::fprintf(stderr, "%s\n", line.c_str());
}

// -- StdoutHandler --

void StdoutHandler::emit(LogLevel level, const char* component,
                          const std::string& message)
{
    if (!accepts(level)) return;
    auto line = format_output(level, component, message);
    std::fprintf(stdout, "%s\n", line.c_str());
    std::fflush(stdout);
}

// -- FileHandler --

FileHandler::FileHandler(const std::string& path, bool append)
    : file_(std::fopen(path.c_str(), append ? "a" : "w"))
{
}

FileHandler::~FileHandler()
{
    if (file_) {
        std::fclose(file_);
        file_ = nullptr;
    }
}

void FileHandler::emit(LogLevel level, const char* component,
                        const std::string& message)
{
    if (!accepts(level) || !file_) return;
    auto line = format_output(level, component, message);
    std::fprintf(file_, "%s\n", line.c_str());
    std::fflush(file_);
}

// -- Logger --

Logger& Logger::instance()
{
    static Logger logger;
    return logger;
}

Logger::Logger()
{
    auto handler = std::make_shared<StderrHandler>();
    handlers_.push_back(handler);
}

void Logger::set_level(LogLevel level)
{
    level_ = level;
}

void Logger::set_component_level(const std::string& component, LogLevel level)
{
    component_levels_[component] = level;
}

void Logger::clear_component_level(const std::string& component)
{
    component_levels_.erase(component);
}

void Logger::clear_all_component_levels()
{
    component_levels_.clear();
}

LogLevel Logger::effective_level(const char* component) const
{
    auto it = component_levels_.find(component);
    if (it != component_levels_.end())
        return it->second;
    return level_;
}

void Logger::add_handler(std::shared_ptr<LogHandler> handler)
{
    handlers_.push_back(std::move(handler));
}

void Logger::remove_handler(const std::shared_ptr<LogHandler>& handler)
{
    handlers_.erase(
        std::remove(handlers_.begin(), handlers_.end(), handler),
        handlers_.end());
}

void Logger::clear_handlers()
{
    handlers_.clear();
}

void Logger::add_sink(LogSink sink)
{
    sinks_.push_back(std::move(sink));
}

void Logger::clear_sinks()
{
    sinks_.clear();
    handlers_.clear();
}

void Logger::log(LogLevel level, const char* component, const std::string& msg)
{
    if (level < effective_level(component))
        return;

    for (auto& handler : handlers_)
        handler->emit(level, component, msg);

    for (auto& sink : sinks_)
        sink(level, component, msg);
}

std::string Logger::format_msg(const char* fmt)
{
    return fmt;
}

} // namespace showroom
