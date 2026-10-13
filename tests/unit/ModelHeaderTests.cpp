// Model headers must be self-contained. This TU includes model/Texture.h first; every other model header is
// included first by its own TU in unit/model_headers/, so each header is compiled alone at least once.
#include "model/Texture.h"

#include "model/ArtObject.h"
#include "model/BackgroundPlane.h"
#include "model/Character.h"
#include "model/Geometry.h"
#include "model/Level.h"
#include "model/TrileEmplacement.h"
#include "model/Vertex.h"

#include <catch2/catch_test_macros.hpp>

#include <type_traits>
#include <vector>

TEST_CASE("Texture animation frames are named AnimationFrame structs", "[model]")
{
    STATIC_REQUIRE(std::is_same_v<fez::Texture::TextureAnimationOffsets, std::vector<fez::AnimationFrame>>);

    fez::Texture texture;
    texture.m_TextureAnimationOffsets.push_back({100u, fez::Vec2f{0.25f, 0.5f}, fez::Vec2f{0.5f, 0.25f}});

    const auto& frame = texture.m_TextureAnimationOffsets.front();
    CHECK(frame.m_Duration == 100u);
    CHECK(frame.m_Offset == fez::Vec2f{0.25f, 0.5f});
    CHECK(frame.m_Size == fez::Vec2f{0.5f, 0.25f});
}

TEST_CASE("TrileEmplacement orientation is spelled m_Orientation", "[model]")
{
    fez::TrileEmplacement emplacement;
    CHECK(emplacement.m_Orientation == 0u);
    emplacement.m_Orientation = 3;
    CHECK(emplacement.m_Orientation == 3u);
}
