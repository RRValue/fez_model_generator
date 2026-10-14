#include "log/Log.h"

#include <catch2/catch_test_macros.hpp>

#include <QtCore/QString>
#include <QtCore/QtLogging>

#include <cstddef>
#include <format>
#include <set>
#include <string>
#include <thread>
#include <utility>
#include <vector>

// The logging facade fez::log: level filtering (debug only when enabled), std::format formatting incl. QString,
// whole-line output from parallel threads, and routing of Qt's own messages. The converter's stderr output with and
// without --verbose is tested end to end in tests/e2e/CliTests.cpp.

namespace
{

struct Line
{
    fez::log::Level m_Level;
    std::string m_Text;
};

// Captures every line written through the facade; restores the previous sink and level on destruction.
class CaptureLog
{
public:
    CaptureLog()
        : m_PreviousLevel(fez::log::level())
    {
        m_PreviousSink = fez::log::setSink([this](fez::log::Level level, std::string_view text) {
            m_Lines.push_back({level, std::string(text)});
        });
    }

    ~CaptureLog()
    {
        fez::log::setSink(std::move(m_PreviousSink));
        fez::log::setLevel(m_PreviousLevel);
    }

    CaptureLog(const CaptureLog&) = delete;
    CaptureLog& operator=(const CaptureLog&) = delete;

    const std::vector<Line>& lines() const
    {
        return m_Lines;
    }

private:
    fez::log::Level m_PreviousLevel;
    fez::log::Sink m_PreviousSink;
    std::vector<Line> m_Lines;
};

} // namespace

TEST_CASE("log: the default level is info, so debug messages are dropped", "[log]")
{
    CHECK(fez::log::level() == fez::log::Level::Info);
    CHECK_FALSE(fez::log::isEnabled(fez::log::Level::Debug));
    CHECK(fez::log::isEnabled(fez::log::Level::Info));

    CaptureLog capture;

    fez::log::debug("hidden {}", 1);
    fez::log::info("shown {}", 2);
    fez::log::warning("warned {}", 3);
    fez::log::error("failed {}", 4);

    REQUIRE(capture.lines().size() == 3);
    CHECK(capture.lines()[0].m_Level == fez::log::Level::Info);
    CHECK(capture.lines()[0].m_Text == "shown 2");
    CHECK(capture.lines()[1].m_Level == fez::log::Level::Warning);
    CHECK(capture.lines()[1].m_Text == "warned 3");
    CHECK(capture.lines()[2].m_Level == fez::log::Level::Error);
    CHECK(capture.lines()[2].m_Text == "failed 4");
}

TEST_CASE("log: debug messages appear when the level is debug", "[log]")
{
    CaptureLog capture;
    fez::log::setLevel(fez::log::Level::Debug);

    CHECK(fez::log::isEnabled(fez::log::Level::Debug));

    fez::log::debug("parsing: {}", QString("art objects/mini_ao.xml"));

    REQUIRE(capture.lines().size() == 1);
    CHECK(capture.lines()[0].m_Level == fez::log::Level::Debug);
    CHECK(capture.lines()[0].m_Text == "parsing: art objects/mini_ao.xml");
}

TEST_CASE("log: a higher level drops lower messages", "[log]")
{
    CaptureLog capture;
    fez::log::setLevel(fez::log::Level::Error);

    fez::log::info("dropped");
    fez::log::warning("dropped");
    fez::log::error("kept");

    REQUIRE(capture.lines().size() == 1);
    CHECK(capture.lines()[0].m_Text == "kept");
}

TEST_CASE("log: QString arguments are formatted as UTF-8", "[log][format]")
{
    CaptureLog capture;

    fez::log::info("{} / {:>5}", QString::fromUtf8("Gr\xC3\xBC\xC3\x9F" "e"), QString("ab"));

    REQUIRE(capture.lines().size() == 1);
    CHECK(capture.lines()[0].m_Text == "Gr\xC3\xBC\xC3\x9F" "e /    ab");
}

TEST_CASE("log: lines from parallel threads are never interleaved", "[log][concurrency]")
{
    // A deliberately non-atomic sink: it appends one character at a time and yields in between. Only the facade's
    // serialization keeps the lines whole.
    std::string output;
    const auto previous_level = fez::log::level();
    auto previous_sink = fez::log::setSink([&output](fez::log::Level, std::string_view text) {
        for(const char c : text)
        {
            output.push_back(c);
            std::this_thread::yield();
        }
        output.push_back('\n');
    });
    fez::log::setLevel(fez::log::Level::Debug);

    constexpr int thread_count = 8;
    constexpr int lines_per_thread = 200;

    {
        std::vector<std::jthread> threads;

        for(int t = 0; t < thread_count; ++t)
        {
            threads.emplace_back([t] {
                for(int i = 0; i < lines_per_thread; ++i)
                {
                    if(i % 2 == 0)
                        fez::log::debug("thread {} line {} {}", t, i, QString("payload-payload-payload"));
                    else
                        fez::log::info("thread {} line {} {}", t, i, QString("payload-payload-payload"));
                }
            });
        }
    }

    fez::log::setSink(std::move(previous_sink));
    fez::log::setLevel(previous_level);

    std::set<std::string> expected;
    for(int t = 0; t < thread_count; ++t)
        for(int i = 0; i < lines_per_thread; ++i)
            expected.insert(std::format("thread {} line {} payload-payload-payload", t, i));

    std::multiset<std::string> actual;
    std::size_t start = 0;
    while(start < output.size())
    {
        const auto end = output.find('\n', start);
        REQUIRE(end != std::string::npos);
        actual.insert(output.substr(start, end - start));
        start = end + 1;
    }

    CHECK(actual.size() == expected.size());
    CHECK(std::set<std::string>(actual.begin(), actual.end()) == expected);
}

TEST_CASE("log: Qt's own messages are routed through the facade", "[log]")
{
    CaptureLog capture;
    const auto previous_handler = fez::log::routeQtMessages();

    qDebug("qt debug %d", 1);
    qInfo("qt info %d", 2);
    qWarning("qt warning %d", 3);

    fez::log::setLevel(fez::log::Level::Debug);
    qDebug("qt debug %d", 4);

    qInstallMessageHandler(previous_handler);

    REQUIRE(capture.lines().size() == 3);
    CHECK(capture.lines()[0].m_Level == fez::log::Level::Info);
    CHECK(capture.lines()[0].m_Text == "qt info 2");
    CHECK(capture.lines()[1].m_Level == fez::log::Level::Warning);
    CHECK(capture.lines()[1].m_Text == "qt warning 3");
    CHECK(capture.lines()[2].m_Level == fez::log::Level::Debug);
    CHECK(capture.lines()[2].m_Text == "qt debug 4");
}
