#include "repository/AssetRepository.h"

#include "support/TestData.h"

#include <catch2/catch_test_macros.hpp>

#include <QtCore/QDir>
#include <QtCore/QString>
#include <QtCore/QTemporaryDir>

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

using fez::AssetRepository;
using fez::Error;
using fez::ErrorKind;
using fez::Geometry;
using fez::Texture;
using fez::TrileSet;

using namespace std::chrono_literals;

namespace
{

// A one-shot signal with a timeout, so that a broken repository makes a test fail instead of hang.
class Gate
{
public:
    void open()
    {
        {
            const std::lock_guard lock(m_Mutex);
            m_Open = true;
        }
        m_Condition.notify_all();
    }

    bool waitFor(std::chrono::milliseconds timeout)
    {
        std::unique_lock lock(m_Mutex);
        return m_Condition.wait_for(lock, timeout, [this] { return m_Open; });
    }

private:
    std::mutex m_Mutex;
    std::condition_variable m_Condition;
    bool m_Open = false;
};

// Loaders that fail the test loudly if a test calls a kind it did not set up.
AssetRepository::Loaders failingLoaders()
{
    AssetRepository::Loaders loaders;
    loaders.trileSet = [](const QString& path) -> std::expected<TrileSet, Error> {
        return std::unexpected(Error{ErrorKind::Io, "unexpected trile set load", path.toStdString()});
    };
    loaders.artObject = [](const QString& path) -> std::expected<Geometry, Error> {
        return std::unexpected(Error{ErrorKind::Io, "unexpected art object load", path.toStdString()});
    };
    loaders.texture = [](const QString& dir, const QString& name, bool) -> std::expected<Texture, Error> {
        return std::unexpected(Error{ErrorKind::Io, "unexpected texture load", (dir + "/" + name).toStdString()});
    };
    return loaders;
}

Geometry namedGeometry(const QString& name)
{
    Geometry geometry;
    geometry.m_Name = name;
    return geometry;
}

// Starts count threads that each call request() once and returns when all of them have finished. The calls are
// released together and the loaders under test block until every thread is about to call the repository.
template<typename Result, typename Request>
std::vector<std::optional<Result>> requestConcurrently(int count, std::atomic<int>& arrived, Request request)
{
    std::vector<std::optional<Result>> results(count);
    std::vector<std::thread> threads;
    threads.reserve(count);

    for(int i = 0; i < count; ++i)
        threads.emplace_back([&, i] {
            arrived.fetch_add(1);
            results[i] = request();
        });

    for(auto& thread : threads)
        thread.join();

    return results;
}

// Blocks the calling (loader) thread until all count threads have arrived and then a little longer, so that the
// other threads are waiting inside the repository when the load finishes.
void waitForAllRequesters(const std::atomic<int>& arrived, int count)
{
    const auto deadline = std::chrono::steady_clock::now() + 10s;

    while(arrived.load() < count && std::chrono::steady_clock::now() < deadline)
        std::this_thread::sleep_for(1ms);

    std::this_thread::sleep_for(250ms);
}

} // namespace

TEST_CASE("AssetRepository: 16 threads requesting the same key load it once and share the pointer", "[repository]")
{
    constexpr int thread_count = 16;

    std::atomic<int> arrived = 0;
    std::atomic<int> load_calls = 0;

    auto loaders = failingLoaders();
    loaders.artObject = [&](const QString&) -> std::expected<Geometry, Error> {
        load_calls.fetch_add(1);
        waitForAllRequesters(arrived, thread_count);
        return namedGeometry("bell");
    };

    AssetRepository repository(QDir::tempPath(), loaders);

    const auto results = requestConcurrently<AssetRepository::Result<Geometry>>(
        thread_count, arrived, [&] { return repository.artObject("bell"); });

    CHECK(load_calls.load() == 1);

    REQUIRE(results[0].has_value());
    REQUIRE(results[0]->has_value());
    const auto first = **results[0];
    CHECK(first->m_Name == "bell");

    for(const auto& result : results)
    {
        REQUIRE(result.has_value());
        REQUIRE(result->has_value());
        CHECK(result->value() == first);
    }

    // later requests get the cached pointer
    const auto again = repository.artObject("bell");
    REQUIRE(again.has_value());
    CHECK(*again == first);
    CHECK(load_calls.load() == 1);
}

TEST_CASE("AssetRepository: different keys load concurrently", "[repository]")
{
    Gate b_loaded;
    std::atomic<bool> a_saw_b = false;
    Gate a_started;

    auto loaders = failingLoaders();
    loaders.trileSet = [&](const QString& path) -> std::expected<TrileSet, Error> {
        if(path.endsWith("/a.xml"))
        {
            a_started.open();
            // only returns early if B's loader runs while this one is still loading
            a_saw_b = b_loaded.waitFor(10s);
            return TrileSet{"a", {}};
        }

        b_loaded.open();
        return TrileSet{"b", {}};
    };

    AssetRepository repository(QDir::tempPath(), loaders);

    std::optional<AssetRepository::Result<TrileSet>> a_result;
    std::thread a_thread([&] { a_result = repository.trileSet("a"); });

    REQUIRE(a_started.waitFor(10s));

    const auto b_result = repository.trileSet("b");
    a_thread.join();

    CHECK(a_saw_b.load());

    REQUIRE(b_result.has_value());
    CHECK((*b_result)->m_Name == "b");
    REQUIRE(a_result.has_value());
    REQUIRE(a_result->has_value());
    CHECK((**a_result)->m_Name == "a");
}

TEST_CASE("AssetRepository: a failed load returns the same Error to all waiters", "[repository]")
{
    constexpr int thread_count = 16;

    std::atomic<int> arrived = 0;
    std::atomic<int> load_calls = 0;

    auto loaders = failingLoaders();
    loaders.texture = [&](const QString&, const QString&, bool) -> std::expected<Texture, Error> {
        const auto call = load_calls.fetch_add(1) + 1;
        waitForAllRequesters(arrived, thread_count);
        return std::unexpected(Error{ErrorKind::FileNotFound, "load #" + std::to_string(call), "missing.png"});
    };

    AssetRepository repository(QDir::tempPath(), loaders);

    const auto results = requestConcurrently<AssetRepository::Result<Texture>>(
        thread_count, arrived, [&] { return repository.texture("background planes", "missing", false); });

    CHECK(load_calls.load() == 1);

    for(const auto& result : results)
    {
        REQUIRE(result.has_value());
        REQUIRE_FALSE(result->has_value());
        CHECK(result->error().kind == ErrorKind::FileNotFound);
        CHECK(result->error().message == "load #1");
        CHECK(result->error().source == "missing.png");
    }
}

TEST_CASE("AssetRepository: failures are not cached", "[repository]")
{
    int load_calls = 0;

    auto loaders = failingLoaders();
    loaders.artObject = [&](const QString&) -> std::expected<Geometry, Error> {
        ++load_calls;
        if(load_calls == 1)
            return std::unexpected(Error{ErrorKind::XmlSyntax, "broken", "ao.xml:1:1"});
        return namedGeometry("ao");
    };

    AssetRepository repository(QDir::tempPath(), loaders);

    const auto first = repository.artObject("ao");
    REQUIRE_FALSE(first.has_value());
    CHECK(first.error().kind == ErrorKind::XmlSyntax);

    const auto second = repository.artObject("ao");
    REQUIRE(second.has_value());
    CHECK((*second)->m_Name == "ao");
    CHECK(load_calls == 2);

    const auto third = repository.artObject("ao");
    REQUIRE(third.has_value());
    CHECK(*third == *second);
    CHECK(load_calls == 2);
}

TEST_CASE("AssetRepository: an exception thrown by a loader reaches the caller and is not cached", "[repository]")
{
    int load_calls = 0;

    auto loaders = failingLoaders();
    loaders.trileSet = [&](const QString&) -> std::expected<TrileSet, Error> {
        if(++load_calls == 1)
            throw std::runtime_error("loader failed");
        return TrileSet{"set", {}};
    };

    AssetRepository repository(QDir::tempPath(), loaders);

    CHECK_THROWS_AS(repository.trileSet("set"), std::runtime_error);

    const auto second = repository.trileSet("set");
    REQUIRE(second.has_value());
    CHECK(load_calls == 2);
}

TEST_CASE("AssetRepository: loaders get paths below the absolute input root", "[repository]")
{
    QTemporaryDir root;
    REQUIRE(root.isValid());
    const auto root_path = QDir(root.path()).absolutePath();

    QString trile_set_path;
    QString art_object_path;
    QString texture_dir;
    QString texture_name;
    std::optional<bool> texture_animated;

    auto loaders = failingLoaders();
    loaders.trileSet = [&](const QString& path) -> std::expected<TrileSet, Error> {
        trile_set_path = path;
        return TrileSet{"set", {}};
    };
    loaders.artObject = [&](const QString& path) -> std::expected<Geometry, Error> {
        art_object_path = path;
        return namedGeometry("ao");
    };
    loaders.texture = [&](const QString& dir, const QString& name, bool animated) -> std::expected<Texture, Error> {
        texture_dir = dir;
        texture_name = name;
        texture_animated = animated;
        return Texture{};
    };

    // a relative, uncleaned root is made absolute and clean, like QDir::absolutePath in the old LevelParser
    AssetRepository repository(root_path + "/levels/..", loaders);

    CHECK(repository.inputRoot() == root_path);

    REQUIRE(repository.trileSet("mini set").has_value());
    CHECK(trile_set_path == root_path + "/trile sets/mini set.xml");

    REQUIRE(repository.artObject("bell.v2").has_value());
    CHECK(art_object_path == root_path + "/art objects/bell.v2.xml");

    REQUIRE(repository.texture("character animations", "npc/walk", true).has_value());
    CHECK(texture_dir == root_path + "/character animations");
    CHECK(texture_name == "npc/walk");
    CHECK(texture_animated == true);
}

TEST_CASE("AssetRepository: the texture key is directory and name; animated is ignored", "[repository]")
{
    int load_calls = 0;
    std::vector<bool> animated_flags;

    auto loaders = failingLoaders();
    loaders.texture = [&](const QString& dir, const QString& name, bool animated) -> std::expected<Texture, Error> {
        ++load_calls;
        animated_flags.push_back(animated);
        Texture texture;
        texture.m_TextureName = dir + "/" + name;
        texture.m_IsAnimated = animated;
        return texture;
    };

    AssetRepository repository(QDir::tempPath(), loaders);

    const auto static_texture = repository.texture("background planes", "water", false);
    const auto animated_request = repository.texture("background planes", "water", true);
    REQUIRE(static_texture.has_value());
    REQUIRE(animated_request.has_value());

    // the first request decides what is loaded (as the old LevelParser cache did)
    CHECK(*animated_request == *static_texture);
    CHECK_FALSE((*animated_request)->m_IsAnimated);
    CHECK(load_calls == 1);

    // another directory or name is another key
    const auto other_dir = repository.texture("character animations", "water", false);
    const auto other_name = repository.texture("background planes", "water2", false);
    REQUIRE(other_dir.has_value());
    REQUIRE(other_name.has_value());
    CHECK(*other_dir != *static_texture);
    CHECK(*other_name != *static_texture);
    CHECK(load_calls == 3);

    // an equivalent directory spelling is the same key
    const auto same_dir = repository.texture("levels/../background planes", "water", true);
    REQUIRE(same_dir.has_value());
    CHECK(*same_dir == *static_texture);
    CHECK(load_calls == 3);

    CHECK(animated_flags == std::vector<bool>{false, false, false});
}

TEST_CASE("AssetRepository: kinds are cached separately", "[repository]")
{
    int trile_set_calls = 0;
    int art_object_calls = 0;

    auto loaders = failingLoaders();
    loaders.trileSet = [&](const QString&) -> std::expected<TrileSet, Error> {
        ++trile_set_calls;
        return TrileSet{"x", {}};
    };
    loaders.artObject = [&](const QString&) -> std::expected<Geometry, Error> {
        ++art_object_calls;
        return namedGeometry("x");
    };

    AssetRepository repository(QDir::tempPath(), loaders);

    REQUIRE(repository.trileSet("x").has_value());
    REQUIRE(repository.artObject("x").has_value());
    REQUIRE(repository.trileSet("x").has_value());
    REQUIRE(repository.artObject("x").has_value());

    CHECK(trile_set_calls == 1);
    CHECK(art_object_calls == 1);
}

TEST_CASE("AssetRepository: default loaders read the mini export", "[repository]")
{
    const auto root = fez::testing::testDataPath(QStringLiteral("mini_export"));
    AssetRepository repository(root);

    SECTION("trile set")
    {
        const auto trile_set = repository.trileSet("mini_set");
        REQUIRE(trile_set.has_value());
        CHECK((*trile_set)->m_Name == "mini_set");
        CHECK((*trile_set)->m_Triles.size() == 2);
    }

    SECTION("art object")
    {
        const auto art_object = repository.artObject("mini_ao");
        REQUIRE(art_object.has_value());
        CHECK((*art_object)->m_Name == "mini_ao");
        CHECK_FALSE((*art_object)->m_Vertices.empty());
    }

    SECTION("static and animated textures")
    {
        const auto plane = repository.texture("background planes", "mini_plane", false);
        REQUIRE(plane.has_value());
        CHECK((*plane)->m_TextureName == "mini_plane.png");
        CHECK_FALSE((*plane)->m_IsAnimated);

        const auto npc = repository.texture("character animations", "mini_npc/idle", true);
        REQUIRE(npc.has_value());
        CHECK((*npc)->m_TextureName == "mini_npc/idle.ani.png");
        CHECK((*npc)->m_IsAnimated);
        CHECK_FALSE((*npc)->m_TextureAnimationOffsets.empty());
    }

    SECTION("missing assets are FileNotFound")
    {
        const auto trile_set = repository.trileSet("no_such_set");
        REQUIRE_FALSE(trile_set.has_value());
        CHECK(trile_set.error().kind == ErrorKind::FileNotFound);
        CHECK(trile_set.error().source == (root + "/trile sets/no_such_set.xml").toStdString());

        const auto art_object = repository.artObject("no_such_ao");
        REQUIRE_FALSE(art_object.has_value());
        CHECK(art_object.error().kind == ErrorKind::FileNotFound);
        CHECK(art_object.error().source == (root + "/art objects/no_such_ao.xml").toStdString());

        const auto texture = repository.texture("background planes", "no_such_plane", false);
        REQUIRE_FALSE(texture.has_value());
        CHECK(texture.error().kind == ErrorKind::FileNotFound);
    }
}
