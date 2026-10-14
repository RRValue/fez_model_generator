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

// TextureCopier copies a texture next to the exported OBJ with the old Writer semantics: QFile::copy never overwrites
// an existing file, and only the destination's own directory is created when missing. In addition, every destination
// is copied at most once per copier (a level references the same texture from many instances).

namespace
{

void writeFile(const QString& path, const QByteArray& content)
{
    QFile file(path);
    REQUIRE(file.open(QIODevice::WriteOnly));
    REQUIRE(file.write(content) == content.size());
}

QByteArray readAll(const QString& path)
{
    QFile file(path);
    REQUIRE(file.open(QIODevice::ReadOnly));
    return file.readAll();
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

    CHECK(copier.copy(source, root.filePath("out/set.png")));
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
    CHECK(copier.copy(source, root.filePath("LEVEL/mini_npc/idle.ani.png")));
    CHECK(readAll(root.filePath("LEVEL/mini_npc/idle.ani.png")) == "npc");
}

TEST_CASE("TextureCopier: only the destination's own directory is created (kept behavior)", "[export][texture]")
{
    QTemporaryDir root;
    REQUIRE(root.isValid());

    const auto source = root.filePath("deep.png");
    writeFile(source, "deep");
    REQUIRE(QDir().mkpath(root.filePath("LEVEL")));

    fez::TextureCopier copier;

    // like the old Writer (QDir::mkdir, not mkpath): two missing levels are not created and the copy fails
    CHECK_FALSE(copier.copy(source, root.filePath("LEVEL/a/b/deep.png")));
    CHECK_FALSE(QFileInfo(root.filePath("LEVEL/a")).exists());
}

TEST_CASE("TextureCopier: never overwrites an existing file", "[export][texture]")
{
    QTemporaryDir root;
    REQUIRE(root.isValid());

    const auto source = root.filePath("set.png");
    writeFile(source, "new");
    const auto destination = root.filePath("set_copy.png");
    writeFile(destination, "old");

    fez::TextureCopier copier;

    CHECK_FALSE(copier.copy(source, destination));
    CHECK(readAll(destination) == "old");
}

TEST_CASE("TextureCopier: copies each destination once", "[export][texture]")
{
    QTemporaryDir root;
    REQUIRE(root.isValid());

    const auto source = root.filePath("set.png");
    writeFile(source, "texture");
    const auto destination = root.filePath("copy.png");

    fez::TextureCopier copier;

    REQUIRE(copier.copy(source, destination));

    // the second request does not touch the file system: the removed copy is not recreated
    REQUIRE(QFile::remove(destination));
    CHECK_FALSE(copier.copy(source, destination));
    CHECK_FALSE(QFileInfo(destination).exists());

    // the same destination spelled differently is the same destination
    CHECK_FALSE(copier.copy(source, root.filePath("./copy.png")));
    CHECK_FALSE(QFileInfo(destination).exists());

    // another copier knows nothing of the first one
    fez::TextureCopier other;
    CHECK(other.copy(source, destination));
    CHECK(QFileInfo(destination).isFile());
}

TEST_CASE("TextureCopier: a failed copy is not remembered", "[export][texture]")
{
    QTemporaryDir root;
    REQUIRE(root.isValid());

    const auto source = root.filePath("late.png");
    const auto destination = root.filePath("copy.png");

    fez::TextureCopier copier;

    CHECK_FALSE(copier.copy(source, destination));
    CHECK_FALSE(QFileInfo(destination).exists());

    writeFile(source, "late");

    CHECK(copier.copy(source, destination));
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
        const auto destination = root.filePath(QString("out/copy%1.png").arg(round));

        fez::TextureCopier copier;
        std::latch start(thread_count);
        std::vector<std::thread> threads;
        std::vector<char> copied(thread_count, 0);
        threads.reserve(thread_count);

        for(int i = 0; i < thread_count; ++i)
            threads.emplace_back([&start, &copier, &source, &destination, &copied, i] {
                start.arrive_and_wait();
                copied[i] = copier.copy(source, destination) ? 1 : 0;
            });

        for(auto& thread : threads)
            thread.join();

        int count = 0;

        for(const auto c : copied)
            count += c;

        CHECK(count == 1);
        CHECK(readAll(destination) == "texture");
    }
}
