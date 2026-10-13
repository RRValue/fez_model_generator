#include "parser/TextureParser.h"

#include "support/TestData.h"

#include <catch2/catch_test_macros.hpp>

#include <QtCore/QByteArray>
#include <QtCore/QDir>
#include <QtCore/QFile>
#include <QtCore/QFileInfo>
#include <QtCore/QTemporaryDir>

#include <string>

using fez::ErrorKind;
using fez::Vec2f;

namespace
{

QString fixtureDir()
{
    return fez::testing::testDataPath(QStringLiteral("parser"));
}

// Writes content to <dir>/<name> (creating parent directories) and returns the path.
QString writeFile(const QTemporaryDir& dir, const QString& name, const QByteArray& content)
{
    const auto path = dir.filePath(name);
    REQUIRE(QDir().mkpath(QFileInfo(path).absolutePath()));
    QFile file(path);
    REQUIRE(file.open(QIODevice::WriteOnly));
    REQUIRE(file.write(content) == content.size());
    return path;
}

// Copies a committed fixture into dir under a new name.
void copyFixture(const QTemporaryDir& dir, const QString& fixture, const QString& name)
{
    const auto target = dir.filePath(name);
    REQUIRE(QDir().mkpath(QFileInfo(target).absolutePath()));
    REQUIRE(QFile::copy(fixtureDir() + QLatin1Char('/') + fixture, target));
}

// An animated texture <name>.ani.png + <name>.xml in dir with the given XML.
void writeAnimated(const QTemporaryDir& dir, const QString& name, const QByteArray& xml)
{
    copyFixture(dir, QStringLiteral("texture_animated.ani.png"), name + QStringLiteral(".ani.png"));
    writeFile(dir, name + QStringLiteral(".xml"), xml);
}

QByteArray animatedXml(const QByteArray& rootAttributes, const QByteArray& frames)
{
    return "<AnimatedTexturePC " + rootAttributes + "><Frames>" + frames + "</Frames></AnimatedTexturePC>";
}

const QByteArray kRootAttributes = "width=\"64\" height=\"32\" actualWidth=\"16\" actualHeight=\"8\"";

QByteArray frame(const QByteArray& duration, const QByteArray& rectangle)
{
    return "<FramePC duration=\"" + duration + "\"><Rectangle " + rectangle + "/></FramePC>";
}

const QByteArray kRectangle = "x=\"0\" y=\"0\" w=\"16\" h=\"8\"";

} // namespace

// --- static textures -----------------------------------------------------------------------------------------------

TEST_CASE("loadTexture reads the size of a static PNG", "[parser]")
{
    const auto texture = fez::loadTexture(fixtureDir(), QStringLiteral("texture_static"), false);
    REQUIRE(texture.has_value());

    CHECK(texture->m_TextureName == QStringLiteral("texture_static.png"));
    CHECK(texture->m_TextureOrgFile == fixtureDir() + QStringLiteral("/texture_static.png"));
    CHECK_FALSE(texture->m_IsAnimated);
    CHECK(texture->m_Width == 3u);
    CHECK(texture->m_Height == 2u);
    CHECK(texture->m_TextureAnimationOffsets.empty());
}

TEST_CASE("loadTexture on a missing static PNG returns FileNotFound", "[parser]")
{
    const auto texture = fez::loadTexture(fixtureDir(), QStringLiteral("no_such_texture"), false);
    REQUIRE_FALSE(texture.has_value());

    CHECK(texture.error().kind == ErrorKind::FileNotFound);
    CHECK(texture.error().source == (fixtureDir() + QStringLiteral("/no_such_texture.png")).toStdString());
}

TEST_CASE("loadTexture on a static PNG that is not an image fails", "[parser]")
{
    const QTemporaryDir dir;
    REQUIRE(dir.isValid());
    writeFile(dir, QStringLiteral("broken.png"), "not a png");

    const auto texture = fez::loadTexture(dir.path(), QStringLiteral("broken"), false);
    REQUIRE_FALSE(texture.has_value());
    CHECK(texture.error().kind == ErrorKind::Io);
}

TEST_CASE("loadTexture does not need an XML file for a static texture", "[parser]")
{
    // texture_animated.xml exists next to it, but a static load only reads <name>.png
    const QTemporaryDir dir;
    REQUIRE(dir.isValid());
    copyFixture(dir, QStringLiteral("texture_static.png"), QStringLiteral("plane.png"));

    const auto texture = fez::loadTexture(dir.path(), QStringLiteral("plane"), false);
    REQUIRE(texture.has_value());
    CHECK(texture->m_Width == 3u);
    CHECK(texture->m_Height == 2u);
}

TEST_CASE("loadTexture keeps a name with a subdirectory", "[parser]")
{
    const QTemporaryDir dir;
    REQUIRE(dir.isValid());
    copyFixture(dir, QStringLiteral("texture_static.png"), QStringLiteral("sub/plane.png"));

    const auto texture = fez::loadTexture(dir.path(), QStringLiteral("sub/plane"), false);
    REQUIRE(texture.has_value());
    CHECK(texture->m_TextureName == QStringLiteral("sub/plane.png"));
    CHECK(texture->m_TextureOrgFile == dir.path() + QStringLiteral("/sub/plane.png"));
}

// --- animated textures ---------------------------------------------------------------------------------------------

TEST_CASE("loadTexture reads an animated texture with two frames", "[parser]")
{
    const auto texture = fez::loadTexture(fixtureDir(), QStringLiteral("texture_animated"), true);
    REQUIRE(texture.has_value());

    CHECK(texture->m_TextureName == QStringLiteral("texture_animated.ani.png"));
    CHECK(texture->m_TextureOrgFile == fixtureDir() + QStringLiteral("/texture_animated.ani.png"));
    CHECK(texture->m_IsAnimated);

    // size from actualWidth / actualHeight, not from the PNG (4x2) or width / height
    CHECK(texture->m_Width == 16u);
    CHECK(texture->m_Height == 8u);

    // offsets and sizes normalized by width (64) / height (32)
    REQUIRE(texture->m_TextureAnimationOffsets.size() == 2);
    const auto& first = texture->m_TextureAnimationOffsets[0];
    CHECK(first.m_Duration == 1000000u);
    CHECK(first.m_Offset == Vec2f(0.0f, 0.0f));
    CHECK(first.m_Size == Vec2f(0.25f, 0.25f));

    const auto& second = texture->m_TextureAnimationOffsets[1];
    CHECK(second.m_Duration == 1500000u);
    CHECK(second.m_Offset == Vec2f(0.25f, 0.25f));
    CHECK(second.m_Size == Vec2f(0.25f, 0.25f));
}

TEST_CASE("parseAnimatedTexture reads size and frames from the XML", "[parser]")
{
    const auto animated = fez::parseAnimatedTexture(fixtureDir() + QStringLiteral("/texture_animated.xml"));
    REQUIRE(animated.has_value());

    CHECK(animated->m_Width == 16u);
    CHECK(animated->m_Height == 8u);
    REQUIRE(animated->m_Frames.size() == 2);
    CHECK(animated->m_Frames[1].m_Duration == 1500000u);
    CHECK(animated->m_Frames[1].m_Offset == Vec2f(0.25f, 0.25f));
}

TEST_CASE("loadTexture on an animated texture without frames fails", "[parser]")
{
    const auto texture = fez::loadTexture(fixtureDir(), QStringLiteral("texture_zero_frames"), true);
    REQUIRE_FALSE(texture.has_value());

    CHECK(texture.error().kind == ErrorKind::MissingElement);
    CHECK(texture.error().source == "AnimatedTexturePC/Frames/FramePC");
}

TEST_CASE("loadTexture on a missing animated PNG returns FileNotFound", "[parser]")
{
    // texture_animated.xml exists, but the animated texture is <name>.ani.png
    const auto texture = fez::loadTexture(fixtureDir(), QStringLiteral("texture_static"), true);
    REQUIRE_FALSE(texture.has_value());

    CHECK(texture.error().kind == ErrorKind::FileNotFound);
    CHECK(texture.error().source == (fixtureDir() + QStringLiteral("/texture_static.ani.png")).toStdString());
}

TEST_CASE("loadTexture on an animated texture without XML returns FileNotFound", "[parser]")
{
    const QTemporaryDir dir;
    REQUIRE(dir.isValid());
    copyFixture(dir, QStringLiteral("texture_animated.ani.png"), QStringLiteral("anim.ani.png"));

    const auto texture = fez::loadTexture(dir.path(), QStringLiteral("anim"), true);
    REQUIRE_FALSE(texture.has_value());

    CHECK(texture.error().kind == ErrorKind::FileNotFound);
    CHECK(texture.error().source == (dir.path() + QStringLiteral("/anim.xml")).toStdString());
}

TEST_CASE("loadTexture does not decode the animated PNG", "[parser]")
{
    // Only the existence of <name>.ani.png is checked (as before).
    const QTemporaryDir dir;
    REQUIRE(dir.isValid());
    writeFile(dir, QStringLiteral("anim.ani.png"), "not a png");
    writeFile(dir, QStringLiteral("anim.xml"), animatedXml(kRootAttributes, frame("7", kRectangle)));

    const auto texture = fez::loadTexture(dir.path(), QStringLiteral("anim"), true);
    REQUIRE(texture.has_value());
    CHECK(texture->m_TextureAnimationOffsets.size() == 1);
}

TEST_CASE("loadTexture on malformed animated XML returns XmlSyntax", "[parser]")
{
    const QTemporaryDir dir;
    REQUIRE(dir.isValid());
    writeAnimated(dir, QStringLiteral("anim"), "<AnimatedTexturePC><Frames>");

    const auto texture = fez::loadTexture(dir.path(), QStringLiteral("anim"), true);
    REQUIRE_FALSE(texture.has_value());
    CHECK(texture.error().kind == ErrorKind::XmlSyntax);
}

TEST_CASE("loadTexture requires the AnimatedTexturePC document element", "[parser]")
{
    const QTemporaryDir dir;
    REQUIRE(dir.isValid());
    writeAnimated(dir, QStringLiteral("anim"), "<AnimatedTexture " + kRootAttributes + "><Frames>" +
                                                   frame("1", kRectangle) + "</Frames></AnimatedTexture>");

    const auto texture = fez::loadTexture(dir.path(), QStringLiteral("anim"), true);
    REQUIRE_FALSE(texture.has_value());
    CHECK(texture.error().kind == ErrorKind::MissingElement);
    CHECK(texture.error().source == "AnimatedTexturePC");
}

TEST_CASE("loadTexture requires all four size attributes as unsigned numbers", "[parser]")
{
    const QTemporaryDir dir;
    REQUIRE(dir.isValid());

    struct Case
    {
        QByteArray attributes;
        ErrorKind kind;
        std::string source;
    };
    const Case cases[] = {
        {"height=\"32\" actualWidth=\"16\" actualHeight=\"8\"", ErrorKind::MissingAttribute, "AnimatedTexturePC@width"},
        {"width=\"64\" actualWidth=\"16\" actualHeight=\"8\"", ErrorKind::MissingAttribute, "AnimatedTexturePC@height"},
        {"width=\"64\" height=\"32\" actualHeight=\"8\"", ErrorKind::MissingAttribute, "AnimatedTexturePC@actualWidth"},
        {"width=\"64\" height=\"32\" actualWidth=\"16\"", ErrorKind::MissingAttribute, "AnimatedTexturePC@actualHeight"},
        {"width=\"64\" height=\"32\" actualWidth=\"-16\" actualHeight=\"8\"", ErrorKind::BadValue, "AnimatedTexturePC@actualWidth"},
        {"width=\"64.5\" height=\"32\" actualWidth=\"16\" actualHeight=\"8\"", ErrorKind::BadValue, "AnimatedTexturePC@width"},
    };

    for (const auto& c : cases)
    {
        INFO(c.attributes.toStdString());
        const auto name = QStringLiteral("anim");
        writeFile(dir, name + QStringLiteral(".ani.png"), "png");
        writeFile(dir, name + QStringLiteral(".xml"), animatedXml(c.attributes, frame("1", kRectangle)));

        const auto texture = fez::loadTexture(dir.path(), name, true);
        REQUIRE_FALSE(texture.has_value());
        CHECK(texture.error().kind == c.kind);
        CHECK(texture.error().source == c.source);
    }
}

TEST_CASE("loadTexture requires a Frames element", "[parser]")
{
    const QTemporaryDir dir;
    REQUIRE(dir.isValid());
    writeAnimated(dir, QStringLiteral("anim"), "<AnimatedTexturePC " + kRootAttributes + "/>");

    const auto texture = fez::loadTexture(dir.path(), QStringLiteral("anim"), true);
    REQUIRE_FALSE(texture.has_value());
    CHECK(texture.error().kind == ErrorKind::MissingElement);
    CHECK(texture.error().source == "AnimatedTexturePC/Frames");
}

TEST_CASE("loadTexture fails on an invalid frame", "[parser]")
{
    const QTemporaryDir dir;
    REQUIRE(dir.isValid());

    struct Case
    {
        QByteArray frame;
        ErrorKind kind;
        std::string source;
    };
    const Case cases[] = {
        {"<FramePC><Rectangle " + kRectangle + "/></FramePC>", ErrorKind::MissingAttribute,
         "AnimatedTexturePC/Frames/FramePC[2]@duration"},
        {frame("-5", kRectangle), ErrorKind::BadValue, "AnimatedTexturePC/Frames/FramePC[2]@duration"},
        {"<FramePC duration=\"1\"/>", ErrorKind::MissingElement, "AnimatedTexturePC/Frames/FramePC[2]/Rectangle"},
        {frame("1", "y=\"0\" w=\"16\" h=\"8\""), ErrorKind::MissingAttribute,
         "AnimatedTexturePC/Frames/FramePC[2]/Rectangle@x"},
        {frame("1", "x=\"0\" y=\"0\" w=\"16\""), ErrorKind::MissingAttribute,
         "AnimatedTexturePC/Frames/FramePC[2]/Rectangle@h"},
        {frame("1", "x=\"0\" y=\"zero\" w=\"16\" h=\"8\""), ErrorKind::BadValue,
         "AnimatedTexturePC/Frames/FramePC[2]/Rectangle@y"},
    };

    for (const auto& c : cases)
    {
        INFO(c.frame.toStdString());
        // the invalid frame comes after a valid one: any invalid frame fails the whole texture
        writeAnimated(dir, QStringLiteral("anim"), animatedXml(kRootAttributes, frame("1", kRectangle) + c.frame));

        const auto texture = fez::loadTexture(dir.path(), QStringLiteral("anim"), true);
        REQUIRE_FALSE(texture.has_value());
        CHECK(texture.error().kind == c.kind);
        CHECK(texture.error().source == c.source);
        REQUIRE(QFile::remove(dir.filePath(QStringLiteral("anim.ani.png"))));
    }
}

TEST_CASE("loadTexture frame iteration starts at the first FramePC and visits every following sibling", "[parser]")
{
    // Kept odd behavior: elements before the first FramePC are skipped, every element after it is read as a frame
    // whatever its name.
    const QTemporaryDir dir;
    REQUIRE(dir.isValid());
    const QByteArray frames = "<Ignored duration=\"9\"><Rectangle x=\"1\" y=\"1\" w=\"1\" h=\"1\"/></Ignored>" +
                              frame("1", kRectangle) +
                              "<Other duration=\"2\"><Rectangle x=\"32\" y=\"16\" w=\"8\" h=\"4\"/></Other>";
    writeAnimated(dir, QStringLiteral("anim"), animatedXml(kRootAttributes, frames));

    const auto texture = fez::loadTexture(dir.path(), QStringLiteral("anim"), true);
    REQUIRE(texture.has_value());
    REQUIRE(texture->m_TextureAnimationOffsets.size() == 2);
    CHECK(texture->m_TextureAnimationOffsets[0].m_Duration == 1u);
    CHECK(texture->m_TextureAnimationOffsets[1].m_Duration == 2u);
    CHECK(texture->m_TextureAnimationOffsets[1].m_Offset == Vec2f(0.5f, 0.5f));
    CHECK(texture->m_TextureAnimationOffsets[1].m_Size == Vec2f(0.125f, 0.125f));
}

TEST_CASE("loadTexture divides frame rectangles as floats by width and height", "[parser]")
{
    const QTemporaryDir dir;
    REQUIRE(dir.isValid());
    writeAnimated(dir, QStringLiteral("anim"),
                  animatedXml("width=\"3\" height=\"7\" actualWidth=\"1\" actualHeight=\"1\"",
                              frame("1", "x=\"1\" y=\"2.5\" w=\"2\" h=\"3\"")));

    const auto texture = fez::loadTexture(dir.path(), QStringLiteral("anim"), true);
    REQUIRE(texture.has_value());
    const auto& f = texture->m_TextureAnimationOffsets[0];
    CHECK(f.m_Offset.x() == 1.0f / 3.0f);
    CHECK(f.m_Offset.y() == 2.5f / 7.0f);
    CHECK(f.m_Size.x() == 2.0f / 3.0f);
    CHECK(f.m_Size.y() == 3.0f / 7.0f);
}
