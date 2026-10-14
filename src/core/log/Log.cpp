#include "log/Log.h"

#include <QtCore/QString>

#include <atomic>
#include <cstdio>
#include <mutex>
#include <print>

namespace fez::log
{

namespace
{

void printToStderr(Level, std::string_view message)
{
    std::println(stderr, "{}", message);
}

struct State
{
    std::atomic<Level> m_Level = Level::Info;
    std::mutex m_Mutex; // guards m_Sink and serializes the calls to it
    Sink m_Sink = printToStderr;
};

State& state()
{
    static State instance;
    return instance;
}

Level toLevel(QtMsgType type)
{
    switch(type)
    {
    case QtDebugMsg:
        return Level::Debug;
    case QtInfoMsg:
        return Level::Info;
    case QtWarningMsg:
        return Level::Warning;
    case QtCriticalMsg:
    case QtFatalMsg:
        return Level::Error;
    }

    return Level::Error;
}

void handleQtMessage(QtMsgType type, const QMessageLogContext&, const QString& message)
{
    const auto level = toLevel(type);

    if(isEnabled(level))
        write(level, message.toUtf8().toStdString());
}

} // namespace

void setLevel(Level minimum)
{
    state().m_Level.store(minimum);
}

Level level()
{
    return state().m_Level.load();
}

bool isEnabled(Level level)
{
    return level >= state().m_Level.load();
}

Sink setSink(Sink sink)
{
    auto& s = state();
    const auto lock = std::scoped_lock(s.m_Mutex);
    auto previous = std::move(s.m_Sink);
    s.m_Sink = sink ? std::move(sink) : Sink(printToStderr);
    return previous;
}

void write(Level level, std::string_view message)
{
    if(!isEnabled(level))
        return;

    auto& s = state();
    const auto lock = std::scoped_lock(s.m_Mutex);
    s.m_Sink(level, message);
}

QtMessageHandler routeQtMessages()
{
    return qInstallMessageHandler(handleQtMessage);
}

} // namespace fez::log
