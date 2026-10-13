#include "geometry/FaceDecoding.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include <optional>

using fez::Vec3f;

// Side indices as they appear in the <Normal> element of FEZ vertices. The vectors are the values the parser writes
// today (the constants are named pos_x, pos_y, ... but point the other way; Task 31 renames them, values stay).

TEST_CASE("decodeFaceNormal maps sides 0-5 to unit axis vectors", "[geometry]")
{
    const auto [side, expected] = GENERATE(table<int, Vec3f>({
        {0, Vec3f{-1, 0, 0}},
        {1, Vec3f{0, -1, 0}},
        {2, Vec3f{0, 0, -1}},
        {3, Vec3f{1, 0, 0}},
        {4, Vec3f{0, 1, 0}},
        {5, Vec3f{0, 0, 1}},
    }));

    CAPTURE(side);
    const auto normal = fez::decodeFaceNormal(side);
    REQUIRE(normal.has_value());
    CHECK(*normal == expected);
}

TEST_CASE("decodeFaceNormal rejects sides outside 0-5", "[geometry]")
{
    const auto side = GENERATE(-1, 6);
    CAPTURE(side);
    CHECK_FALSE(fez::decodeFaceNormal(side).has_value());
}

TEST_CASE("faceUvOffset shifts U by the side index for sides 0-5", "[geometry]")
{
    const auto [side, expected] = GENERATE(table<int, float>({
        {0, 0.0f},
        {1, 1.0f},
        {2, 2.0f},
        {3, 3.0f},
        {4, 4.0f},
        {5, 5.0f},
    }));

    CAPTURE(side);
    CHECK(fez::faceUvOffset(side) == expected);
}

TEST_CASE("faceUvOffset is 0 for sides outside 0-5", "[geometry]")
{
    const auto side = GENERATE(-1, 6);
    CAPTURE(side);
    CHECK(fez::faceUvOffset(side) == 0.0f);
}
