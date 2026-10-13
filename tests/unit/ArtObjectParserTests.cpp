#include "parser/ArtObjectParser.h"

#include "support/TestData.h"

#include <catch2/catch_test_macros.hpp>

#include <QtCore/QDir>
#include <QtCore/QFile>
#include <QtCore/QFileInfo>
#include <QtCore/QTemporaryDir>

using fez::ErrorKind;
using fez::Vec2f;
using fez::Vec3f;

namespace
{

QString fixture(const QString& name)
{
    return fez::testing::testDataPath(QStringLiteral("parser/") + name);
}

} // namespace

TEST_CASE("parseArtObject reads a valid art object", "[parser]")
{
    const auto path = fixture(QStringLiteral("artobject_valid.xml"));
    const auto geometry = fez::parseArtObject(path);
    REQUIRE(geometry.has_value());

    // Name is the file base name (not the name attribute); the texture is <name>.png next to the XML.
    CHECK(geometry->m_Name == QStringLiteral("artobject_valid"));
    CHECK(geometry->m_Texture.m_TextureName == QStringLiteral("artobject_valid.png"));
    CHECK(geometry->m_Texture.m_TextureOrgFile == QFileInfo(path).absolutePath() + QStringLiteral("/artobject_valid.png"));

    // Geometry comes from ShaderInstancedIndexedPrimitives (parseGeometry).
    REQUIRE(geometry->m_Vertices.size() == 3);
    CHECK(geometry->m_Vertices[0].m_Position == Vec3f{-0.5f, -0.5f, 0.5f});
    CHECK(geometry->m_Vertices[0].m_Normal == Vec3f{0, 0, -1});
    CHECK(geometry->m_Vertices[0].m_TextureCoordinate == Vec2f{2.0f, 0.0f});
    CHECK(geometry->m_Vertices[2].m_TextureCoordinate == Vec2f{2.125f, 0.25f});
    CHECK(geometry->m_Indices == fez::Geometry::Indices{0, 1, 2});

    // Everything else keeps its default.
    CHECK(geometry->m_Opacity == 1.0f);
    CHECK(geometry->m_DoubleSided);
    CHECK_FALSE(geometry->m_IsPlane);
}

TEST_CASE("parseArtObject names the geometry after the base name up to the first dot", "[parser]")
{
    QTemporaryDir dir;
    REQUIRE(dir.isValid());
    const auto path = dir.filePath(QStringLiteral("dotted.name.xml"));
    REQUIRE(QFile::copy(fixture(QStringLiteral("artobject_valid.xml")), path));

    const auto geometry = fez::parseArtObject(path);
    REQUIRE(geometry.has_value());
    CHECK(geometry->m_Name == QStringLiteral("dotted"));
    CHECK(geometry->m_Texture.m_TextureName == QStringLiteral("dotted.png"));
    CHECK(geometry->m_Texture.m_TextureOrgFile == QFileInfo(path).absolutePath() + QStringLiteral("/dotted.png"));
}

TEST_CASE("parseArtObject rejects a document without ArtObject root", "[parser]")
{
    const auto geometry = fez::parseArtObject(fixture(QStringLiteral("artobject_missing_root.xml")));
    REQUIRE_FALSE(geometry.has_value());
    CHECK(geometry.error().kind == ErrorKind::MissingElement);
    CHECK(geometry.error().source == "ArtObject");
}

TEST_CASE("parseArtObject rejects an art object without geometry", "[parser]")
{
    const auto geometry = fez::parseArtObject(fixture(QStringLiteral("artobject_missing_geometry.xml")));
    REQUIRE_FALSE(geometry.has_value());
    CHECK(geometry.error().kind == ErrorKind::MissingElement);
    CHECK(geometry.error().source == "ArtObject/ShaderInstancedIndexedPrimitives");
}

TEST_CASE("parseArtObject reports a missing file as FileNotFound", "[parser]")
{
    const auto path = fixture(QStringLiteral("artobject_does_not_exist.xml"));
    const auto geometry = fez::parseArtObject(path);
    REQUIRE_FALSE(geometry.has_value());
    CHECK(geometry.error().kind == ErrorKind::FileNotFound);
    CHECK(geometry.error().source == path.toStdString());
}

TEST_CASE("parseArtObject reports malformed XML as XmlSyntax", "[parser]")
{
    QTemporaryDir dir;
    REQUIRE(dir.isValid());
    const auto path = dir.filePath(QStringLiteral("broken.xml"));
    {
        QFile file(path);
        REQUIRE(file.open(QIODevice::WriteOnly));
        file.write("<ArtObject><ShaderInstancedIndexedPrimitives></ArtObject>");
    }

    const auto geometry = fez::parseArtObject(path);
    REQUIRE_FALSE(geometry.has_value());
    CHECK(geometry.error().kind == ErrorKind::XmlSyntax);
}
