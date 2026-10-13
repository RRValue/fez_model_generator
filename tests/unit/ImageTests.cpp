#include "geometry/Image.h"

#include <catch2/catch_test_macros.hpp>

#include <QtGui/QColor>
#include <QtGui/QImage>

// hasTranslucentPixels decides whether the writer adds an opacity texture to the material of a plane: true only when
// the image has an alpha channel and at least one pixel has alpha below 1.

TEST_CASE("hasTranslucentPixels is false for an image without alpha channel", "[geometry]")
{
    QImage image(4, 3, QImage::Format_RGB32);
    image.fill(QColor(10, 20, 30));
    REQUIRE_FALSE(image.hasAlphaChannel());

    CHECK_FALSE(fez::hasTranslucentPixels(image));
}

TEST_CASE("hasTranslucentPixels is false for a fully opaque RGBA image", "[geometry]")
{
    QImage image(4, 3, QImage::Format_ARGB32);
    image.fill(QColor(10, 20, 30, 255));
    REQUIRE(image.hasAlphaChannel());

    CHECK_FALSE(fez::hasTranslucentPixels(image));
}

TEST_CASE("hasTranslucentPixels is true for an RGBA image with one translucent pixel", "[geometry]")
{
    QImage image(4, 3, QImage::Format_ARGB32);
    image.fill(QColor(10, 20, 30, 255));

    SECTION("first pixel")
    {
        image.setPixelColor(0, 0, QColor(10, 20, 30, 254));
        CHECK(fez::hasTranslucentPixels(image));
    }

    SECTION("last pixel")
    {
        image.setPixelColor(3, 2, QColor(10, 20, 30, 0));
        CHECK(fez::hasTranslucentPixels(image));
    }
}

TEST_CASE("hasTranslucentPixels is false for a null image", "[geometry]")
{
    const QImage image;
    REQUIRE(image.isNull());

    CHECK_FALSE(fez::hasTranslucentPixels(image));
}
