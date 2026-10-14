#include "level/ResolveLevel.h"

#include "repository/AssetRepository.h"

#include <catch2/catch_test_macros.hpp>

#include <QtCore/QDir>
#include <QtCore/QFileInfo>
#include <QtCore/QString>
#include <QtCore/QStringList>

#include <expected>
#include <map>
#include <string>
#include <utility>

using fez::AnimationFrame;
using fez::AssetRepository;
using fez::BackgroundPlaneDescription;
using fez::Error;
using fez::ErrorKind;
using fez::Geometry;
using fez::LevelDescription;
using fez::NpcDescription;
using fez::Texture;
using fez::TrileEmplacement;
using fez::TrileSet;
using fez::Vec2f;
using fez::Vec3f;

namespace
{

Geometry namedGeometry(const QString& name)
{
    Geometry geometry;
    geometry.m_Name = name;
    return geometry;
}

Texture staticTexture(const QString& name, unsigned int width, unsigned int height)
{
    Texture texture;
    texture.m_TextureName = name;
    texture.m_Width = width;
    texture.m_Height = height;
    return texture;
}

// Two frames: the first one at offset (0.25, 0.5) with size (0.25, 0.5), the second one elsewhere.
Texture animatedTexture(const QString& name, unsigned int width, unsigned int height)
{
    Texture texture = staticTexture(name, width, height);
    texture.m_IsAnimated = true;
    texture.m_TextureAnimationOffsets = {
        AnimationFrame{100, Vec2f{0.25f, 0.5f}, Vec2f{0.25f, 0.5f}},
        AnimationFrame{100, Vec2f{0.5f, 0.0f}, Vec2f{0.5f, 0.5f}},
    };
    return texture;
}

// An in-memory FEZ export: assets that are not in the maps fail with FileNotFound, every load is recorded.
struct FakeExport
{
    std::map<QString, TrileSet> trileSets;
    std::map<QString, Geometry> artObjects;
    std::map<QString, Texture> textures;  // key "<dir name>/<texture name>", e.g. "background planes/sky"

    QStringList loads;  // "<kind>:<key>" in call order; textures also record ":animated" or ":static"

    AssetRepository::Loaders loaders()
    {
        AssetRepository::Loaders result;
        result.trileSet = [this](const QString& path) -> std::expected<TrileSet, Error> {
            const auto name = QFileInfo(path).completeBaseName();
            loads.push_back("trileSet:" + name);
            if(const auto found = trileSets.find(name); found != trileSets.cend())
                return found->second;
            return std::unexpected(Error{ErrorKind::FileNotFound, "no trile set", path.toStdString()});
        };
        result.artObject = [this](const QString& path) -> std::expected<Geometry, Error> {
            const auto name = QFileInfo(path).completeBaseName();
            loads.push_back("artObject:" + name);
            if(const auto found = artObjects.find(name); found != artObjects.cend())
                return found->second;
            return std::unexpected(Error{ErrorKind::FileNotFound, "no art object", path.toStdString()});
        };
        result.texture = [this](const QString& dir, const QString& name, bool animated) -> std::expected<Texture, Error> {
            const auto key = QDir(dir).dirName() + "/" + name;
            loads.push_back("texture:" + key + (animated ? ":animated" : ":static"));
            if(const auto found = textures.find(key); found != textures.cend())
                return found->second;
            return std::unexpected(Error{ErrorKind::FileNotFound, "no texture", (dir + "/" + name).toStdString()});
        };
        return result;
    }
};

TrileEmplacement emplacement(int id, Vec3f position)
{
    TrileEmplacement result;
    result.m_Id = id;
    result.m_Position = position;
    return result;
}

fez::ArtObject artObjectInstance(const QString& name, Vec3f position)
{
    fez::ArtObject result;
    result.m_Name = name;
    result.m_Position = position;
    return result;
}

BackgroundPlaneDescription plane(const QString& textureName, bool animated)
{
    BackgroundPlaneDescription result;
    result.m_TextureName = textureName;
    result.m_Animated = animated;
    result.m_Opacity = 0.5f;
    result.m_DoubleSided = true;
    result.m_Position = Vec3f{1.0f, 2.0f, 3.0f};
    result.m_Rotation = fez::QuaternionF{0.0f, 1.0f, 0.0f, 0.0f};
    result.m_Scale = Vec3f{2.0f, 2.0f, 2.0f};
    return result;
}

NpcDescription npc(const QString& name, NpcDescription::Actions actions)
{
    NpcDescription result;
    result.m_Name = name;
    result.m_Actions = std::move(actions);
    result.m_Position = Vec3f{4.0f, 5.0f, 6.0f};
    return result;
}

// A fake export with everything the level of validDescription() references.
FakeExport validExport()
{
    FakeExport fake;

    TrileSet trile_set;
    trile_set.m_Name = "village";
    trile_set.m_Triles = {{1, namedGeometry("trile 1")}, {2, namedGeometry("trile 2")}, {3, namedGeometry("trile 3")}};
    fake.trileSets["village"] = trile_set;

    fake.artObjects["bell"] = namedGeometry("bell geometry");
    fake.artObjects["tree"] = namedGeometry("tree geometry");

    fake.textures["background planes/sky"] = staticTexture("sky", 40, 72);
    fake.textures["background planes/water/falls"] = animatedTexture("falls", 64, 32);
    fake.textures["character animations/owl/idle"] = animatedTexture("owl", 40, 24);

    return fake;
}

LevelDescription validDescription()
{
    LevelDescription description;
    description.m_Name = "VILLAGE";
    description.m_TrileSetName = "village";
    description.m_TrileEmplacements = {emplacement(2, Vec3f{0, 0, 0}), emplacement(-1, Vec3f{1, 0, 0}),
                                       emplacement(1, Vec3f{2, 0, 0}), emplacement(2, Vec3f{3, 0, 0})};
    description.m_ArtObjects = {artObjectInstance("bell", Vec3f{0, 1, 0}), artObjectInstance("tree", Vec3f{0, 2, 0}),
                                artObjectInstance("bell", Vec3f{0, 3, 0})};
    description.m_BackgroundPlanes = {plane("sky", false), plane("water/falls", true)};
    description.m_Npcs = {npc("owl", {{"Idle", "idle"}, {"Walk", "walk"}})};
    return description;
}

void checkQuad(const Geometry& quad, Vec2f size, Vec2f uvOffset, Vec2f uvSize, float depth)
{
    REQUIRE(quad.m_Vertices.size() == 4);
    CHECK(quad.m_Indices == Geometry::Indices{0, 1, 2, 2, 1, 3});
    CHECK(quad.m_IsPlane);

    const float w = size.x();
    const float h = size.y();
    const Vec2f uv_end = uvOffset + uvSize;

    CHECK(quad.m_Vertices[0].m_Position == Vec3f{-w / 2, h / 2, depth});
    CHECK(quad.m_Vertices[1].m_Position == Vec3f{-w / 2, -h / 2, depth});
    CHECK(quad.m_Vertices[2].m_Position == Vec3f{w / 2, h / 2, depth});
    CHECK(quad.m_Vertices[3].m_Position == Vec3f{w / 2, -h / 2, depth});

    CHECK(quad.m_Vertices[0].m_TextureCoordinate == Vec2f{uvOffset.x(), 1.0f - uvOffset.y()});
    CHECK(quad.m_Vertices[1].m_TextureCoordinate == Vec2f{uvOffset.x(), 1.0f - uv_end.y()});
    CHECK(quad.m_Vertices[2].m_TextureCoordinate == Vec2f{uv_end.x(), 1.0f - uvOffset.y()});
    CHECK(quad.m_Vertices[3].m_TextureCoordinate == Vec2f{uv_end.x(), 1.0f - uv_end.y()});
}

} // namespace

TEST_CASE("resolveLevel: a valid description resolves every reference", "[level][resolve]")
{
    auto fake = validExport();
    AssetRepository repository(QDir::tempPath(), fake.loaders());

    const auto level = fez::resolveLevel(validDescription(), repository);

    REQUIRE(level.has_value());

    CHECK(level->m_LevelName == "VILLAGE");
    CHECK(level->m_TrileSetName == "village");

    // emplacements are kept as described, including id -1
    REQUIRE(level->m_TrileEmplacements.size() == 4);
    CHECK(level->m_TrileEmplacements[1].m_Id == -1);
    CHECK(level->m_TrileEmplacements[3].m_Position == Vec3f{3, 0, 0});

    // one geometry per used trile id
    REQUIRE(level->m_TrileGeometries.size() == 2);
    CHECK(level->m_TrileGeometries.at(1).m_Name == "trile 1");
    CHECK(level->m_TrileGeometries.at(2).m_Name == "trile 2");

    REQUIRE(level->m_ArtObjects.size() == 3);
    CHECK(level->m_ArtObjects[2].m_Name == "bell");
    CHECK(level->m_ArtObjects[2].m_Position == Vec3f{0, 3, 0});

    REQUIRE(level->m_ArtObjectGeometries.size() == 2);
    CHECK(level->m_ArtObjectGeometries.at("bell").m_Name == "bell geometry");
    CHECK(level->m_ArtObjectGeometries.at("tree").m_Name == "tree geometry");

    // assets are requested in the old order: trile set, art objects (each name once), plane textures, NPC textures
    CHECK(fake.loads == QStringList{"trileSet:village", "artObject:bell", "artObject:tree",
                                    "texture:background planes/sky:static",
                                    "texture:background planes/water/falls:animated",
                                    "texture:character animations/owl/idle:animated"});
}

TEST_CASE("resolveLevel: background planes get their texture and a quad", "[level][resolve]")
{
    auto fake = validExport();
    AssetRepository repository(QDir::tempPath(), fake.loaders());

    const auto level = fez::resolveLevel(validDescription(), repository);

    REQUIRE(level.has_value());
    REQUIRE(level->m_BackgroundPlanes.size() == 2);

    const auto& sky = level->m_BackgroundPlanes[0];

    CHECK(sky.m_Name == "sky");
    CHECK(sky.m_Position == Vec3f{1.0f, 2.0f, 3.0f});
    CHECK(sky.m_Rotation.coeffs() == fez::QuaternionF{0.0f, 1.0f, 0.0f, 0.0f}.coeffs());
    CHECK(sky.m_Scale == Vec3f{2.0f, 2.0f, 2.0f});
    CHECK(sky.m_Geometry.m_Name == "sky");
    CHECK(sky.m_Geometry.m_Texture.m_TextureName == "sky");
    CHECK(sky.m_Geometry.m_Opacity == 0.5f);
    CHECK(sky.m_Geometry.m_DoubleSided);

    SECTION("a static texture covers the whole quad; the size uses integer division (40 / 16 = 2, 72 / 16 = 4)")
    {
        checkQuad(sky.m_Geometry, Vec2f{2.0f, 4.0f}, Vec2f{0.0f, 0.0f}, Vec2f{1.0f, 1.0f}, 0.05f);
    }

    SECTION("an animated plane uses the first animation frame")
    {
        const auto& falls = level->m_BackgroundPlanes[1];

        CHECK(falls.m_Name == "water/falls");
        CHECK(falls.m_Geometry.m_Name == "falls");
        CHECK(falls.m_Geometry.m_Texture.m_IsAnimated);
        checkQuad(falls.m_Geometry, Vec2f{4.0f, 2.0f}, Vec2f{0.25f, 0.5f}, Vec2f{0.25f, 0.5f}, 0.05f);
    }
}

TEST_CASE("resolveLevel: NPCs get the texture of their first action and a quad", "[level][resolve]")
{
    auto fake = validExport();
    AssetRepository repository(QDir::tempPath(), fake.loaders());

    const auto level = fez::resolveLevel(validDescription(), repository);

    REQUIRE(level.has_value());
    REQUIRE(level->m_Characters.size() == 1);

    const auto& owl = level->m_Characters[0];

    CHECK(owl.m_Name == "owl");
    CHECK(owl.m_Position == Vec3f{4.0f, 5.0f, 6.0f});
    CHECK(owl.m_Geometry.m_Name == "owl");
    CHECK(owl.m_Geometry.m_Texture.m_IsAnimated);

    // float division (40 / 16 = 2.5, 24 / 16 = 1.5), first frame, no depth offset; opacity/sidedness keep defaults
    checkQuad(owl.m_Geometry, Vec2f{2.5f, 1.5f}, Vec2f{0.25f, 0.5f}, Vec2f{0.25f, 0.5f}, 0.0f);
    CHECK(owl.m_Geometry.m_Opacity == 1.0f);
    CHECK(owl.m_Geometry.m_DoubleSided);
}

TEST_CASE("resolveLevel: trile id -1 is skipped even when the set has no trile -1", "[level][resolve]")
{
    auto fake = validExport();
    AssetRepository repository(QDir::tempPath(), fake.loaders());

    auto description = validDescription();
    description.m_TrileEmplacements = {emplacement(-1, Vec3f{0, 0, 0})};

    const auto level = fez::resolveLevel(description, repository);

    REQUIRE(level.has_value());
    CHECK(level->m_TrileEmplacements.size() == 1);
    CHECK(level->m_TrileGeometries.empty());
}

TEST_CASE("resolveLevel: a missing art object fails the level", "[level][resolve]")
{
    auto fake = validExport();
    fake.artObjects.erase("tree");
    AssetRepository repository(QDir::tempPath(), fake.loaders());

    const auto level = fez::resolveLevel(validDescription(), repository);

    REQUIRE_FALSE(level.has_value());
    CHECK(level.error().kind == ErrorKind::FileNotFound);
    CHECK(QString::fromStdString(level.error().source).endsWith("art objects/tree.xml"));

    // nothing after the failing art object is loaded
    CHECK(fake.loads == QStringList{"trileSet:village", "artObject:bell", "artObject:tree"});
}

TEST_CASE("resolveLevel: a failed trile set fails the level", "[level][resolve]")
{
    auto fake = validExport();
    fake.trileSets.clear();
    AssetRepository repository(QDir::tempPath(), fake.loaders());

    const auto level = fez::resolveLevel(validDescription(), repository);

    REQUIRE_FALSE(level.has_value());
    CHECK(level.error().kind == ErrorKind::FileNotFound);
    CHECK(fake.loads == QStringList{"trileSet:village"});
}

TEST_CASE("resolveLevel: a trile set without triles fails the level", "[level][resolve]")
{
    auto fake = validExport();
    fake.trileSets["village"].m_Triles.clear();
    AssetRepository repository(QDir::tempPath(), fake.loaders());

    auto description = validDescription();

    SECTION("with emplacements")
    {
    }

    SECTION("even when the level places no trile")
    {
        description.m_TrileEmplacements.clear();
    }

    const auto level = fez::resolveLevel(description, repository);

    REQUIRE_FALSE(level.has_value());
    CHECK(level.error().kind == ErrorKind::BadValue);
    CHECK(QString::fromStdString(level.error().source).endsWith("trile sets/village.xml"));
    CHECK(fake.loads == QStringList{"trileSet:village"});
}

TEST_CASE("resolveLevel: a trile id that is not in the set fails the level", "[level][resolve]")
{
    auto fake = validExport();
    AssetRepository repository(QDir::tempPath(), fake.loaders());

    auto description = validDescription();
    description.m_TrileEmplacements.push_back(emplacement(7, Vec3f{0, 0, 0}));

    const auto level = fez::resolveLevel(description, repository);

    REQUIRE_FALSE(level.has_value());
    CHECK(level.error().kind == ErrorKind::BadValue);
    CHECK(level.error().message.find("7") != std::string::npos);
    CHECK(fake.loads == QStringList{"trileSet:village"});
}

TEST_CASE("resolveLevel: a failed background plane texture fails the level", "[level][resolve]")
{
    auto fake = validExport();
    fake.textures.erase("background planes/sky");
    AssetRepository repository(QDir::tempPath(), fake.loaders());

    const auto level = fez::resolveLevel(validDescription(), repository);

    REQUIRE_FALSE(level.has_value());
    CHECK(level.error().kind == ErrorKind::FileNotFound);
    CHECK(fake.loads == QStringList{"trileSet:village", "artObject:bell", "artObject:tree",
                                    "texture:background planes/sky:static"});
}

TEST_CASE("resolveLevel: a failed NPC texture fails the level", "[level][resolve]")
{
    auto fake = validExport();
    fake.textures.erase("character animations/owl/idle");
    AssetRepository repository(QDir::tempPath(), fake.loaders());

    const auto level = fez::resolveLevel(validDescription(), repository);

    REQUIRE_FALSE(level.has_value());
    CHECK(level.error().kind == ErrorKind::FileNotFound);
    CHECK(fake.loads.back() == "texture:character animations/owl/idle:animated");
}

TEST_CASE("resolveLevel: an NPC without actions fails the level", "[level][resolve]")
{
    auto fake = validExport();
    AssetRepository repository(QDir::tempPath(), fake.loaders());

    auto description = validDescription();
    description.m_Npcs = {npc("owl", {})};

    const auto level = fez::resolveLevel(description, repository);

    REQUIRE_FALSE(level.has_value());
    CHECK(level.error().kind == ErrorKind::MissingElement);
    CHECK_FALSE(fake.loads.back().startsWith("texture:character animations"));
}
