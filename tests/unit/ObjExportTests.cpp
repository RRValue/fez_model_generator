#include "export/ObjExport.h"
#include "export/SceneBuilder.h"

#include "model/Geometry.h"

#include <catch2/catch_test_macros.hpp>

#include <QtCore/QDir>
#include <QtCore/QFile>
#include <QtCore/QFileInfo>
#include <QtCore/QString>
#include <QtCore/QTemporaryDir>

#include <assimp/scene.h>

#include <latch>
#include <memory>
#include <thread>
#include <vector>

// exportObj writes one scene as <dir>/<name>.obj plus <dir>/<name>.mtl and reports every failure as an Export error;
// makeDirectory creates the output directories race-free for the parallel conversion.

namespace
{

fez::Geometry triangle()
{
    fez::Geometry geometry;
    geometry.m_Name = "tri";
    geometry.m_Vertices = {
        {.m_Position = {0.0f, 0.0f, 0.0f}, .m_Normal = {0.0f, 0.0f, 1.0f}, .m_TextureCoordinate = {0.0f, 0.0f}},
        {.m_Position = {1.0f, 0.0f, 0.0f}, .m_Normal = {0.0f, 0.0f, 1.0f}, .m_TextureCoordinate = {1.0f, 0.0f}},
        {.m_Position = {0.0f, 1.0f, 0.0f}, .m_Normal = {0.0f, 0.0f, 1.0f}, .m_TextureCoordinate = {0.0f, 1.0f}},
    };
    geometry.m_Indices = {0, 1, 2};
    geometry.m_Texture.m_TextureName = "tri.png";
    return geometry;
}

std::unique_ptr<aiScene> triangleScene()
{
    fez::SceneBuilder builder;
    const auto id = builder.addMesh(triangle());
    REQUIRE(id.has_value());
    builder.addNode("tri", {}, *id);
    return builder.build();
}

QByteArray readAll(const QString& path)
{
    QFile file(path);
    REQUIRE(file.open(QIODevice::ReadOnly));
    return file.readAll();
}

} // namespace

TEST_CASE("exportObj: writes <name>.obj and <name>.mtl into a missing directory", "[export]")
{
    QTemporaryDir root;
    REQUIRE(root.isValid());

    const auto scene = triangleScene();
    const auto dir = root.filePath("lv_export/LEVEL");

    const auto result = fez::exportObj(*scene, dir, "LEVEL");

    REQUIRE(result.has_value());
    CHECK(QFileInfo(dir + "/LEVEL.obj").isFile());
    CHECK(QFileInfo(dir + "/LEVEL.mtl").isFile());

    const auto obj = readAll(dir + "/LEVEL.obj");
    CHECK(obj.contains("mtllib LEVEL.mtl"));
    CHECK(obj.contains("f "));

    const auto mtl = readAll(dir + "/LEVEL.mtl");
    CHECK(mtl.contains("map_Kd tri.png"));

    // exactly these two files, nothing else
    CHECK(QDir(dir).entryList(QDir::AllEntries | QDir::NoDotAndDotDot).size() == 2);
}

TEST_CASE("exportObj: a directory below a regular file is an Export error", "[export]")
{
    QTemporaryDir root;
    REQUIRE(root.isValid());

    QFile blocker(root.filePath("blocker"));
    REQUIRE(blocker.open(QIODevice::WriteOnly));
    blocker.close();

    const auto scene = triangleScene();
    const auto dir = root.filePath("blocker/sub");

    const auto result = fez::exportObj(*scene, dir, "name");

    REQUIRE_FALSE(result.has_value());
    CHECK(result.error().kind == fez::ErrorKind::Export);
    CHECK_FALSE(result.error().message.empty());
    CHECK(QString::fromStdString(result.error().source).contains("blocker/sub"));
    CHECK_FALSE(QFileInfo(dir).exists());
}

TEST_CASE("exportObj: an .obj path that cannot be written is an Export error", "[export]")
{
    QTemporaryDir root;
    REQUIRE(root.isValid());

    // <dir>/name.obj is a directory, so the exporter cannot open it for writing
    const auto dir = root.filePath("out");
    REQUIRE(QDir().mkpath(dir + "/name.obj"));

    const auto scene = triangleScene();
    const auto result = fez::exportObj(*scene, dir, "name");

    REQUIRE_FALSE(result.has_value());
    CHECK(result.error().kind == fez::ErrorKind::Export);
    CHECK_FALSE(result.error().message.empty());
    CHECK(QString::fromStdString(result.error().source).endsWith("out/name.obj"));
}

TEST_CASE("makeDirectory: creates missing parents and accepts an existing directory", "[export]")
{
    QTemporaryDir root;
    REQUIRE(root.isValid());

    const auto dir = root.filePath("a/b/c");

    CHECK(fez::makeDirectory(dir).has_value());
    CHECK(QFileInfo(dir).isDir());
    CHECK(fez::makeDirectory(dir).has_value());
}

// Levels are exported in parallel; every level creates its own output directory below a shared parent
// (<output>/lv_export) that does not exist before the first one. QDir::mkpath failed for a thread that created the
// missing parent at the same moment as another thread, which used to drop that level's output silently
// (fez_mode-777.20). Directory creation must be race-free by design.
TEST_CASE("makeDirectory: concurrent calls create sibling directories below a missing parent", "[export]")
{
    QTemporaryDir root;
    REQUIRE(root.isValid());

    constexpr int rounds = 20;
    constexpr int thread_count = 16;

    QStringList missing;
    int failures = 0;

    for(int round = 0; round < rounds; ++round)
    {
        const auto parent = root.filePath(QString("round%1/lv_export").arg(round));

        std::latch start(thread_count);
        std::vector<std::thread> threads;
        std::vector<char> succeeded(thread_count, 0);
        threads.reserve(thread_count);

        for(int i = 0; i < thread_count; ++i)
            threads.emplace_back([&start, &parent, &succeeded, i] {
                start.arrive_and_wait();
                succeeded[i] = fez::makeDirectory(parent + QString("/LEVEL_%1").arg(i)).has_value() ? 1 : 0;
            });

        for(auto& thread : threads)
            thread.join();

        for(int i = 0; i < thread_count; ++i)
        {
            const auto dir = parent + QString("/LEVEL_%1").arg(i);

            if(!succeeded[i])
                ++failures;

            if(!QDir(dir).exists())
                missing.push_back(dir);
        }
    }

    INFO(missing.join('\n').toStdString());
    CHECK(missing.isEmpty());
    CHECK(failures == 0);
}

TEST_CASE("makeDirectory: concurrent calls for the same directory all succeed", "[export]")
{
    QTemporaryDir root;
    REQUIRE(root.isValid());

    constexpr int rounds = 20;
    constexpr int thread_count = 16;

    int failures = 0;

    for(int round = 0; round < rounds; ++round)
    {
        const auto dir = root.filePath(QString("round%1/ts_export/SET").arg(round));

        std::latch start(thread_count);
        std::vector<std::thread> threads;
        std::vector<char> succeeded(thread_count, 0);
        threads.reserve(thread_count);

        for(int i = 0; i < thread_count; ++i)
            threads.emplace_back([&start, &dir, &succeeded, i] {
                start.arrive_and_wait();
                succeeded[i] = fez::makeDirectory(dir).has_value() ? 1 : 0;
            });

        for(auto& thread : threads)
            thread.join();

        for(int i = 0; i < thread_count; ++i)
            if(!succeeded[i])
                ++failures;

        CHECK(QFileInfo(dir).isDir());
    }

    CHECK(failures == 0);
}

TEST_CASE("exportObj: concurrent exports into sibling directories below a missing parent all succeed", "[export]")
{
    QTemporaryDir root;
    REQUIRE(root.isValid());

    constexpr int rounds = 5;
    constexpr int thread_count = 16;

    std::vector<std::unique_ptr<aiScene>> scenes;

    for(int i = 0; i < thread_count; ++i)
        scenes.push_back(triangleScene());

    QStringList missing;
    int failures = 0;

    for(int round = 0; round < rounds; ++round)
    {
        const auto parent = root.filePath(QString("round%1/lv_export").arg(round));

        std::latch start(thread_count);
        std::vector<std::thread> threads;
        std::vector<char> succeeded(thread_count, 0);
        threads.reserve(thread_count);

        for(int i = 0; i < thread_count; ++i)
            threads.emplace_back([&start, &parent, &scenes, &succeeded, i] {
                start.arrive_and_wait();
                const auto name = QString("LEVEL_%1").arg(i);
                succeeded[i] = fez::exportObj(*scenes[i], parent + "/" + name, name).has_value() ? 1 : 0;
            });

        for(auto& thread : threads)
            thread.join();

        for(int i = 0; i < thread_count; ++i)
        {
            const auto obj = parent + QString("/LEVEL_%1/LEVEL_%1.obj").arg(i);

            if(!succeeded[i])
                ++failures;

            if(!QFileInfo(obj).isFile())
                missing.push_back(obj);
        }
    }

    INFO(missing.join('\n').toStdString());
    CHECK(missing.isEmpty());
    CHECK(failures == 0);
}
