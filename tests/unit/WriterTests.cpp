#include "writer/GeometryWriter.h"

#include <catch2/catch_test_macros.hpp>

#include <QtCore/QDir>
#include <QtCore/QString>
#include <QtCore/QTemporaryDir>

#include <latch>
#include <thread>
#include <vector>

// Levels are written in parallel; every Writer creates its own output directory below a shared parent
// (<output>/lv_export) that does not exist before the first one. QDir::mkpath fails for a thread that creates the
// missing parent at the same moment as another thread, which used to drop that level's output.
TEST_CASE("Writer: concurrent writers create sibling directories below a missing parent", "[writer]")
{
    QTemporaryDir root;
    REQUIRE(root.isValid());

    constexpr int rounds = 20;
    constexpr int thread_count = 16;

    QStringList missing;

    for(int round = 0; round < rounds; ++round)
    {
        const auto parent = root.filePath(QString("round%1/lv_export").arg(round));

        std::latch start(thread_count);
        std::vector<std::thread> threads;
        threads.reserve(thread_count);

        for(int i = 0; i < thread_count; ++i)
            threads.emplace_back([&start, &parent, i] {
                start.arrive_and_wait();
                fez::GeometryWriter writer(parent + QString("/LEVEL_%1").arg(i));
            });

        for(auto& thread : threads)
            thread.join();

        for(int i = 0; i < thread_count; ++i)
        {
            const auto dir = parent + QString("/LEVEL_%1").arg(i);

            if(!QDir(dir).exists())
                missing.push_back(dir);
        }
    }

    INFO(missing.join('\n').toStdString());
    CHECK(missing.isEmpty());
}
