#include "export/TextureCopier.h"

#include <catch2/catch_test_macros.hpp>

#include <QtCore/QByteArray>
#include <QtCore/QDir>
#include <QtCore/QFile>
#include <QtCore/QFileInfo>
#include <QtCore/QString>
#include <QtCore/QTemporaryDir>

#include <latch>
#include <thread>
#include <vector>

// TextureCopier copies a texture next to the exported OBJ. Every destination is copied at most once per copier (a level
// references the same texture from many instances); an existing destination file is replaced (like the OBJ/MTL files),
// so converting into an existing output folder gives the same outcome as a fresh one. Missing directories of the
// destination are created, and a failed copy is returned as an Io error.

namespace
{

void writeFile(const QString& path, const QByteArray& content)
{
    QFile file(path);
    REQUIRE(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
    REQUIRE(file.write(content) == content.size());
}

QByteArray readAll(const QString& path)
{
    QFile file(path);
    REQUIRE(file.open(QIODevice::ReadOnly));
    return file.readAll();
}

// True when the copy succeeded and this call copied the file.
bool copied(const std::expected<bool, fez::Error>& outcome)
{
    return outcome.has_value() && *outcome;
}

} // namespace

TEST_CASE("TextureCopier: copies the source to the destination", "[export][texture]")
{
    QTemporaryDir root;
    REQUIRE(root.isValid());

    const auto source = root.filePath("set.png");
    writeFile(source, "texture");
    REQUIRE(QDir().mkpath(root.filePath("out")));

    fez::TextureCopier copier;

    CHECK(copied(copier.copy(source, root.filePath("out/set.png"))));
    CHECK(readAll(root.filePath("out/set.png")) == "texture");
}

TEST_CASE("TextureCopier: creates the missing directory of the destination", "[export][texture]")
{
    QTemporaryDir root;
    REQUIRE(root.isValid());

    const auto source = root.filePath("idle.ani.png");
    writeFile(source, "npc");
    REQUIRE(QDir().mkpath(root.filePath("LEVEL")));

    fez::TextureCopier copier;

    // an NPC texture name has a subdirectory: "<npc>/<action>.ani.png"
    CHECK(copied(copier.copy(source, root.filePath("LEVEL/mini_npc/idle.ani.png"))));
    CHECK(readAll(root.filePath("LEVEL/mini_npc/idle.ani.png")) == "npc");
}

TEST_CASE("TextureCopier: creates every missing directory of the destination", "[export][texture]")
{
    QTemporaryDir root;
    REQUIRE(root.isValid());

    const auto source = root.filePath("deep.png");
    writeFile(source, "deep");

    fez::TextureCopier copier;

    // nothing below root exists: LEVEL, a and b are created
    CHECK(copied(copier.copy(source, root.filePath("LEVEL/a/b/deep.png"))));
    CHECK(readAll(root.filePath("LEVEL/a/b/deep.png")) == "deep");
}

TEST_CASE("TextureCopier: replaces an existing destination file", "[export][texture]")
{
    QTemporaryDir root;
    REQUIRE(root.isValid());

    const auto source = root.filePath("set.png");
    writeFile(source, "new");
    const auto destination = root.filePath("set_copy.png");
    writeFile(destination, "old content");

    fez::TextureCopier copier;

    CHECK(copied(copier.copy(source, destination)));
    CHECK(readAll(destination) == "new");
}

TEST_CASE("TextureCopier: a missing source is an Io error", "[export][texture]")
{
    QTemporaryDir root;
    REQUIRE(root.isValid());

    const auto source = root.filePath("missing.png");
    const auto destination = root.filePath("out/missing.png");

    fez::TextureCopier copier;

    const auto outcome = copier.copy(source, destination);

    REQUIRE_FALSE(outcome.has_value());
    CHECK(outcome.error().kind == fez::ErrorKind::Io);
    CHECK(outcome.error().source == source.toStdString());
    CHECK_FALSE(outcome.error().message.empty());
    CHECK_FALSE(QFileInfo::exists(destination));
}

TEST_CASE("TextureCopier: a destination that cannot be replaced is an Io error", "[export][texture]")
{
    QTemporaryDir root;
    REQUIRE(root.isValid());

    const auto source = root.filePath("set.png");
    writeFile(source, "texture");

    // the destination is a directory (with content), which a texture copy must not remove
    const auto destination = root.filePath("out/set.png");
    REQUIRE(QDir().mkpath(destination));
    writeFile(destination + "/keep.txt", "keep");

    fez::TextureCopier copier;

    const auto outcome = copier.copy(source, destination);

    REQUIRE_FALSE(outcome.has_value());
    CHECK(outcome.error().kind == fez::ErrorKind::Io);
    CHECK(outcome.error().source == destination.toStdString());
    CHECK(readAll(destination + "/keep.txt") == "keep");
}

TEST_CASE("TextureCopier: a destination directory that cannot be created is an Io error", "[export][texture]")
{
    QTemporaryDir root;
    REQUIRE(root.isValid());

    const auto source = root.filePath("idle.ani.png");
    writeFile(source, "npc");

    // the NPC directory is a regular file
    writeFile(root.filePath("mini_npc"), "file");

    fez::TextureCopier copier;

    const auto outcome = copier.copy(source, root.filePath("mini_npc/idle.ani.png"));

    REQUIRE_FALSE(outcome.has_value());
    CHECK(outcome.error().kind == fez::ErrorKind::Io);
    CHECK(readAll(root.filePath("mini_npc")) == "file");
}

TEST_CASE("TextureCopier: copies each destination once", "[export][texture]")
{
    QTemporaryDir root;
    REQUIRE(root.isValid());

    const auto source = root.filePath("set.png");
    writeFile(source, "texture");
    const auto destination = root.filePath("copy.png");

    fez::TextureCopier copier;

    REQUIRE(copied(copier.copy(source, destination)));

    // the second request does not touch the file system: the removed copy is not recreated
    REQUIRE(QFile::remove(destination));
    auto again = copier.copy(source, destination);
    REQUIRE(again.has_value());
    CHECK_FALSE(*again);
    CHECK_FALSE(QFileInfo(destination).exists());

    // the same destination spelled differently is the same destination
    again = copier.copy(source, root.filePath("./copy.png"));
    REQUIRE(again.has_value());
    CHECK_FALSE(*again);
    CHECK_FALSE(QFileInfo(destination).exists());

    // another copier knows nothing of the first one
    fez::TextureCopier other;
    CHECK(copied(other.copy(source, destination)));
    CHECK(QFileInfo(destination).isFile());
}

TEST_CASE("TextureCopier: a failed copy is not remembered", "[export][texture]")
{
    QTemporaryDir root;
    REQUIRE(root.isValid());

    const auto source = root.filePath("late.png");
    const auto destination = root.filePath("copy.png");

    fez::TextureCopier copier;

    CHECK_FALSE(copier.copy(source, destination).has_value());
    CHECK_FALSE(QFileInfo(destination).exists());

    writeFile(source, "late");

    CHECK(copied(copier.copy(source, destination)));
    CHECK(readAll(destination) == "late");
}

TEST_CASE("TextureCopier: concurrent requests for one destination copy it once", "[export][texture]")
{
    QTemporaryDir root;
    REQUIRE(root.isValid());

    const auto source = root.filePath("set.png");
    writeFile(source, "texture");
    REQUIRE(QDir().mkpath(root.filePath("out")));

    constexpr int rounds = 20;
    constexpr int thread_count = 16;

    for(int round = 0; round < rounds; ++round)
    {
        INFO("round " << round);
        const auto destination = root.filePath(QString("out/copy%1.png").arg(round));

        // every other round starts with a stale destination file that must be replaced exactly once
        if(round % 2 == 1)
            writeFile(destination, "stale texture content");

        fez::TextureCopier copier;
        std::latch start(thread_count);
        std::vector<std::thread> threads;
        std::vector<char> results(thread_count, 0);
        std::vector<char> failures(thread_count, 0);
        threads.reserve(thread_count);

        for(int i = 0; i < thread_count; ++i)
            threads.emplace_back([&start, &copier, &source, &destination, &results, &failures, i] {
                start.arrive_and_wait();
                const auto outcome = copier.copy(source, destination);
                failures[i] = outcome.has_value() ? 0 : 1;
                results[i] = copied(outcome) ? 1 : 0;
            });

        for(auto& thread : threads)
            thread.join();

        int count = 0;
        int failed = 0;

        for(int i = 0; i < thread_count; ++i)
        {
            count += results[i];
            failed += failures[i];
        }

        CHECK(failed == 0);
        CHECK(count == 1);
        CHECK(readAll(destination) == "texture");
    }
}
