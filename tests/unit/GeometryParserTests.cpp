#include "parser/GeometryParser.h"

#include "support/TestData.h"
#include "xml/Xml.h"

#include <catch2/catch_test_macros.hpp>

#include <QtCore/QByteArray>
#include <QtXml/QDomDocument>

#include <cstddef>
#include <limits>

using fez::ErrorKind;
using fez::Vec2f;
using fez::Vec3f;

namespace
{

QDomDocument loadFixture(const QString& name)
{
    auto document = fez::loadXml(fez::testing::testDataPath(QStringLiteral("parser/") + name));
    REQUIRE(document.has_value());
    return *document;
}

QDomDocument parseInline(const char* xml)
{
    auto document = fez::parseXml(QByteArray(xml), QStringLiteral("inline"));
    REQUIRE(document.has_value());
    return *document;
}

} // namespace

TEST_CASE("parseGeometry reads a quad with two faces", "[parser]")
{
    const auto document = loadFixture(QStringLiteral("geometry_quad.xml"));
    const auto geometry = fez::parseGeometry(document.documentElement());
    REQUIRE(geometry.has_value());

    REQUIRE(geometry->m_Vertices.size() == 4);

    // Position as written; normal decoded from the side index; V flipped; U shifted by the side index.
    CHECK(geometry->m_Vertices[0].m_Position == Vec3f{0.5f, -0.5f, 0.5f});
    CHECK(geometry->m_Vertices[0].m_Normal == Vec3f{1, 0, 0});
    CHECK(geometry->m_Vertices[0].m_TextureCoordinate == Vec2f{3.0f, 0.0f});

    CHECK(geometry->m_Vertices[1].m_Position == Vec3f{0.5f, -0.5f, -0.5f});
    CHECK(geometry->m_Vertices[1].m_TextureCoordinate == Vec2f{3.125f, 0.0f});

    CHECK(geometry->m_Vertices[2].m_Position == Vec3f{0.5f, 0.5f, -0.5f});
    CHECK(geometry->m_Vertices[2].m_TextureCoordinate == Vec2f{3.125f, 0.75f});

    CHECK(geometry->m_Vertices[3].m_Position == Vec3f{-1.5f, 0.5f, 0.5f});
    CHECK(geometry->m_Vertices[3].m_Normal == Vec3f{-1, 0, 0});
    CHECK(geometry->m_Vertices[3].m_TextureCoordinate == Vec2f{0.0625f, 1.0f});

    // Indices are stored as written: no reordering, winding unchanged.
    const auto expected_indices = fez::Geometry::Indices{0, 2, 1, 0, 3, 2};
    CHECK(geometry->m_Indices == expected_indices);

    // Only vertices and indices are set; everything else keeps its default.
    CHECK(geometry->m_Name.isEmpty());
    CHECK(geometry->m_Opacity == 1.0f);
    CHECK(geometry->m_DoubleSided);
    CHECK_FALSE(geometry->m_IsPlane);
}

TEST_CASE("parseGeometry rejects an index count not divisible by 3", "[parser]")
{
    const auto document = loadFixture(QStringLiteral("geometry_index_count.xml"));
    const auto geometry = fez::parseGeometry(document.documentElement());
    REQUIRE_FALSE(geometry.has_value());
    CHECK(geometry.error().kind == ErrorKind::BadValue);
    CHECK(geometry.error().source == "ShaderInstancedIndexedPrimitives/Indices");
}

TEST_CASE("parseGeometry rejects a Vector3 without z", "[parser]")
{
    const auto document = loadFixture(QStringLiteral("geometry_missing_vector3_attribute.xml"));
    const auto geometry = fez::parseGeometry(document.documentElement());
    REQUIRE_FALSE(geometry.has_value());
    CHECK(geometry.error().kind == ErrorKind::MissingAttribute);
    CHECK(geometry.error().source ==
          "ShaderInstancedIndexedPrimitives/Vertices/VertexPositionNormalTextureInstance/Position/Vector3@z");
}

TEST_CASE("parseGeometry rejects a normal side outside 0-5", "[parser]")
{
    const auto document = loadFixture(QStringLiteral("geometry_invalid_normal.xml"));
    const auto geometry = fez::parseGeometry(document.documentElement());
    REQUIRE_FALSE(geometry.has_value());
    CHECK(geometry.error().kind == ErrorKind::BadValue);
    CHECK(geometry.error().source ==
          "ShaderInstancedIndexedPrimitives/Vertices/VertexPositionNormalTextureInstance/Normal");
}

TEST_CASE("parseGeometry rejects a null or differently named element", "[parser]")
{
    const auto null_result = fez::parseGeometry(QDomElement());
    REQUIRE_FALSE(null_result.has_value());
    CHECK(null_result.error().kind == ErrorKind::MissingElement);

    const auto document = parseInline("<Geometry><Vertices/><Indices/></Geometry>");
    const auto named_result = fez::parseGeometry(document.documentElement());
    REQUIRE_FALSE(named_result.has_value());
    CHECK(named_result.error().kind == ErrorKind::BadValue);
    CHECK(named_result.error().source == "Geometry");
}

TEST_CASE("parseGeometry requires Vertices and Indices but accepts them empty", "[parser]")
{
    const auto no_indices = parseInline("<ShaderInstancedIndexedPrimitives><Vertices/></ShaderInstancedIndexedPrimitives>");
    const auto missing = fez::parseGeometry(no_indices.documentElement());
    REQUIRE_FALSE(missing.has_value());
    CHECK(missing.error().kind == ErrorKind::MissingElement);
    CHECK(missing.error().source == "ShaderInstancedIndexedPrimitives/Indices");

    const auto empty = parseInline("<ShaderInstancedIndexedPrimitives><Vertices/><Indices/></ShaderInstancedIndexedPrimitives>");
    const auto geometry = fez::parseGeometry(empty.documentElement());
    REQUIRE(geometry.has_value());
    CHECK(geometry->m_Vertices.empty());
    CHECK(geometry->m_Indices.empty());
}

TEST_CASE("parseGeometry rejects a non-numeric index and wraps a negative one", "[parser]")
{
    const auto bad = parseInline("<ShaderInstancedIndexedPrimitives><Vertices/>"
                                 "<Indices><Index>0</Index><Index>x</Index><Index>1</Index></Indices>"
                                 "</ShaderInstancedIndexedPrimitives>");
    const auto bad_result = fez::parseGeometry(bad.documentElement());
    REQUIRE_FALSE(bad_result.has_value());
    CHECK(bad_result.error().kind == ErrorKind::BadValue);
    CHECK(bad_result.error().source == "ShaderInstancedIndexedPrimitives/Indices/Index[2]");

    // The index text is read as int and stored in size_t, as before.
    const auto negative = parseInline("<ShaderInstancedIndexedPrimitives><Vertices/>"
                                      "<Indices><Index>-1</Index><Index>0</Index><Index>1</Index></Indices>"
                                      "</ShaderInstancedIndexedPrimitives>");
    const auto negative_result = fez::parseGeometry(negative.documentElement());
    REQUIRE(negative_result.has_value());
    CHECK(negative_result->m_Indices[0] == std::numeric_limits<std::size_t>::max());
}

TEST_CASE("parseGeometry treats every element after the first vertex or index as one", "[parser]")
{
    // As before: iteration starts at the first named child and then visits all following sibling elements.
    const auto vertices = parseInline("<ShaderInstancedIndexedPrimitives><Vertices>"
                                      "<VertexPositionNormalTextureInstance>"
                                      "<Position><Vector3 x='0' y='0' z='0'/></Position><Normal>0</Normal>"
                                      "<TextureCoord><Vector2 x='0' y='0'/></TextureCoord>"
                                      "</VertexPositionNormalTextureInstance>"
                                      "<Other/>"
                                      "</Vertices><Indices/></ShaderInstancedIndexedPrimitives>");
    const auto vertices_result = fez::parseGeometry(vertices.documentElement());
    REQUIRE_FALSE(vertices_result.has_value());
    CHECK(vertices_result.error().kind == ErrorKind::MissingElement);
    CHECK(vertices_result.error().source == "ShaderInstancedIndexedPrimitives/Vertices/Other/Position");

    const auto indices = parseInline("<ShaderInstancedIndexedPrimitives><Vertices/><Indices>"
                                     "<Skipped>7</Skipped><Index>0</Index><Other>1</Other><Index>2</Index>"
                                     "</Indices></ShaderInstancedIndexedPrimitives>");
    const auto indices_result = fez::parseGeometry(indices.documentElement());
    REQUIRE(indices_result.has_value());
    const auto expected_indices = fez::Geometry::Indices{0, 1, 2};
    CHECK(indices_result->m_Indices == expected_indices);
}
