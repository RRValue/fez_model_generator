#include "geometry/TexturedQuad.h"

#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <cstddef>
#include <vector>

using fez::Geometry;
using fez::UvRect;
using fez::Vec2f;
using fez::Vec3f;

// Quads for background planes and NPCs: corner order top-left, bottom-left, top-right, bottom-right; two triangles
// 0,1,2 and 2,1,3; normal +Z; V flipped (v = 1 - y) because FEZ texture coordinates start at the top.

TEST_CASE("makeTexturedQuad builds 4 vertices and the indices 0,1,2,2,1,3", "[geometry]")
{
    const Geometry quad = fez::makeTexturedQuad(Vec2f{4.0f, 2.0f}, UvRect{Vec2f{0.0f, 0.0f}, Vec2f{1.0f, 1.0f}}, 0.0f);

    REQUIRE(quad.m_Vertices.size() == 4);
    CHECK(quad.m_Indices == std::vector<std::size_t>{0, 1, 2, 2, 1, 3});
    CHECK(quad.m_IsPlane);
}

TEST_CASE("makeTexturedQuad centres the quad at the origin with normal +Z", "[geometry]")
{
    const Geometry quad = fez::makeTexturedQuad(Vec2f{4.0f, 2.0f}, UvRect{Vec2f{0.0f, 0.0f}, Vec2f{1.0f, 1.0f}}, 0.0f);
    REQUIRE(quad.m_Vertices.size() == 4);

    CHECK(quad.m_Vertices[0].m_Position == Vec3f{-2.0f, 1.0f, 0.0f});
    CHECK(quad.m_Vertices[1].m_Position == Vec3f{-2.0f, -1.0f, 0.0f});
    CHECK(quad.m_Vertices[2].m_Position == Vec3f{2.0f, 1.0f, 0.0f});
    CHECK(quad.m_Vertices[3].m_Position == Vec3f{2.0f, -1.0f, 0.0f});

    for(const auto& vertex : quad.m_Vertices)
        CHECK(vertex.m_Normal == Vec3f{0.0f, 0.0f, 1.0f});
}

TEST_CASE("makeTexturedQuad flips V of the UV rectangle", "[geometry]")
{
    // First frame of an animated texture: offset (0.25, 0.5), size (0.25, 0.125).
    const Geometry quad = fez::makeTexturedQuad(Vec2f{1.0f, 1.0f}, UvRect{Vec2f{0.25f, 0.5f}, Vec2f{0.25f, 0.125f}}, 0.0f);
    REQUIRE(quad.m_Vertices.size() == 4);

    CHECK(quad.m_Vertices[0].m_TextureCoordinate == Vec2f{0.25f, 0.5f});
    CHECK(quad.m_Vertices[1].m_TextureCoordinate == Vec2f{0.25f, 0.375f});
    CHECK(quad.m_Vertices[2].m_TextureCoordinate == Vec2f{0.5f, 0.5f});
    CHECK(quad.m_Vertices[3].m_TextureCoordinate == Vec2f{0.5f, 0.375f});
}

TEST_CASE("makeTexturedQuad maps the full texture to V 1 at the top and 0 at the bottom", "[geometry]")
{
    const Geometry quad = fez::makeTexturedQuad(Vec2f{1.0f, 1.0f}, UvRect{Vec2f{0.0f, 0.0f}, Vec2f{1.0f, 1.0f}}, 0.0f);
    REQUIRE(quad.m_Vertices.size() == 4);

    CHECK(quad.m_Vertices[0].m_TextureCoordinate == Vec2f{0.0f, 1.0f});
    CHECK(quad.m_Vertices[1].m_TextureCoordinate == Vec2f{0.0f, 0.0f});
    CHECK(quad.m_Vertices[2].m_TextureCoordinate == Vec2f{1.0f, 1.0f});
    CHECK(quad.m_Vertices[3].m_TextureCoordinate == Vec2f{1.0f, 0.0f});
}

TEST_CASE("makeTexturedQuad moves the quad out along +Z by the depth offset", "[geometry]")
{
    const Geometry quad = fez::makeTexturedQuad(Vec2f{4.0f, 2.0f}, UvRect{Vec2f{0.0f, 0.0f}, Vec2f{1.0f, 1.0f}}, 0.05f);
    REQUIRE(quad.m_Vertices.size() == 4);

    CHECK(quad.m_Vertices[0].m_Position == Vec3f{-2.0f, 1.0f, 0.05f});
    CHECK(quad.m_Vertices[1].m_Position == Vec3f{-2.0f, -1.0f, 0.05f});
    CHECK(quad.m_Vertices[2].m_Position == Vec3f{2.0f, 1.0f, 0.05f});
    CHECK(quad.m_Vertices[3].m_Position == Vec3f{2.0f, -1.0f, 0.05f});
}

TEST_CASE("makeTexturedQuad keeps the sign of zero coordinates exactly as the parsers did", "[geometry]")
{
    // Size 0 (before Task 28, any texture narrower than 16 px). Without depth offset (NPCs) the left X is
    // -0; with depth offset (background planes) the old code added the offset vector, which turned -0 into +0.
    const UvRect uv{Vec2f{0.0f, 0.0f}, Vec2f{1.0f, 1.0f}};

    const Geometry npc = fez::makeTexturedQuad(Vec2f{0.0f, 0.0f}, uv, 0.0f);
    REQUIRE(npc.m_Vertices.size() == 4);
    CHECK(std::signbit(npc.m_Vertices[0].m_Position.x()));
    CHECK(std::signbit(npc.m_Vertices[1].m_Position.y()));

    const Geometry plane = fez::makeTexturedQuad(Vec2f{0.0f, 0.0f}, uv, 0.05f);
    REQUIRE(plane.m_Vertices.size() == 4);
    CHECK_FALSE(std::signbit(plane.m_Vertices[0].m_Position.x()));
    CHECK_FALSE(std::signbit(plane.m_Vertices[1].m_Position.y()));
}
