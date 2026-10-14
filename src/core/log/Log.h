#pragma once

#include "format/QStringFormatter.h"

#include <QtCore/QtLogging>

#include <format>
#include <functional>
#include <string_view>
#include <utility>

// fez::log: a thin logging facade. Messages are formatted with std::format (QString arguments via
// format/QStringFormatter.h) and written as whole lines to stderr with std::print. Messages below the current level
// are dropped without being formatted; the default level is Info, so debug messages appear only after
// setLevel(Level::Debug) (the converter does that for --verbose).
//
// Thread safety: every function may be called from any thread. Each message is passed to the sink under one lock, so
// lines from parallel workers never interleave.
namespace fez::log
{

enum class Level
{
    Debug,
    Info,
    Warning,
    Error,
};

// Receives one message (without line end) that passed the level filter. Calls are serialized by the facade.
using Sink = std::function<void(Level level, std::string_view message)>;

// Messages below minimum are dropped. Default: Level::Info.
void setLevel(Level minimum);
Level level();
bool isEnabled(Level level);

// Replaces the output and returns the previous sink. An empty sink restores the default (std::print to stderr, one
// line per message). Meant for tests.
Sink setSink(Sink sink);

// Passes message to the sink when level is enabled.
void write(Level level, std::string_view message);

// Installs a Qt message handler that routes Qt's debug/info/warning/critical/fatal messages (including Qt's own) through
// the facade as Debug/Info/Warning/Error/Error. Returns the previous handler.
QtMessageHandler routeQtMessages();

template<typename... Args>
void log(Level level, std::format_string<Args...> format, Args&&... args)
{
    if(isEnabled(level))
        write(level, std::format(format, std::forward<Args>(args)...));
}

template<typename... Args>
void debug(std::format_string<Args...> format, Args&&... args)
{
    log(Level::Debug, format, std::forward<Args>(args)...);
}

template<typename... Args>
void info(std::format_string<Args...> format, Args&&... args)
{
    log(Level::Info, format, std::forward<Args>(args)...);
}

template<typename... Args>
void warning(std::format_string<Args...> format, Args&&... args)
{
    log(Level::Warning, format, std::forward<Args>(args)...);
}

template<typename... Args>
void error(std::format_string<Args...> format, Args&&... args)
{
    log(Level::Error, format, std::forward<Args>(args)...);
}

} // namespace fez::log
