#include "parser/TrileSetParser.h"

#include "support/TestData.h"

#include <catch2/catch_test_macros.hpp>

#include <QtCore/QByteArray>
#include <QtCore/QDir>
#include <QtCore/QFile>
#include <QtCore/QFileInfo>
#include <QtCore/QStringList>
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

// Writes content to <dir>/<name> and returns the path.
QString writeFile(const QTemporaryDir& dir, const QString& name, const QByteArray& content)
{
    const auto path = dir.filePath(name);
    QFile file(path);
    REQUIRE(file.open(QIODevice::WriteOnly));
    REQUIRE(file.write(content) == content.size());
    return path;
}

// A trile entry whose geometry is one triangle.
QByteArray trileEntry(const QByteArray& key, const QByteArray& name)
{
    return "<TrileEntry key=\"" + key + "\"><Trile name=\"" + name + "\"><Geometry>"
           "<ShaderInstancedIndexedPrimitives><Vertices>"
           "<VertexPositionNormalTextureInstance><Position><Vector3 x=\"0\" y=\"0\" z=\"0\"/></Position><Normal>0</Normal>"
           "<TextureCoord><Vector2 x=\"0\" y=\"0\"/></TextureCoord></VertexPositionNormalTextureInstance>"
           "<VertexPositionNormalTextureInstance><Position><Vector3 x=\"1\" y=\"0\" z=\"0\"/></Position><Normal>0</Normal>"
           "<TextureCoord><Vector2 x=\"1\" y=\"0\"/></TextureCoord></VertexPositionNormalTextureInstance>"
           "<VertexPositionNormalTextureInstance><Position><Vector3 x=\"0\" y=\"1\" z=\"0\"/></Position><Normal>0</Normal>"
           "<TextureCoord><Vector2 x=\"0\" y=\"1\"/></TextureCoord></VertexPositionNormalTextureInstance>"
           "</Vertices><Indices><Index>0</Index><Index>1</Index><Index>2</Index></Indices>"
           "</ShaderInstancedIndexedPrimitives></Geometry></Trile></TrileEntry>";
}

QByteArray trileSet(const QByteArray& entries)
{
    return "<TrileSet name=\"inline\"><Triles>" + entries + "</Triles></TrileSet>";
}

// Every entry (files and directories, recursively) below dir, relative to it.
QStringList listTree(const QString& dir)
{
    QStringList entries;
    const QDir root(dir);
    for (const auto& info : root.entryInfoList(QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Hidden | QDir::System))
    {
        entries << info.fileName();
        if (info.isDir())
            for (const auto& child : listTree(info.absoluteFilePath()))
                entries << info.fileName() + QLatin1Char('/') + child;
    }
    return entries;
}

} // namespace

TEST_CASE("parseTrileSet reads two triles with keys and names", "[parser]")
{
    const auto path = fixture(QStringLiteral("trileset_valid.xml"));
    const auto set = fez::parseTrileSet(path);
    REQUIRE(set.has_value());

    // The set name comes from the name attribute, not from the file name.
    CHECK(set->m_Name == QStringLiteral("Fixture Set"));

    REQUIRE(set->m_Triles.size() == 2);
    REQUIRE(set->m_Triles.contains(3));
    REQUIRE(set->m_Triles.contains(7));

    const auto& cube = set->m_Triles.at(3);
    const auto& slab = set->m_Triles.at(7);

    // Geometry name "<key>_<name>"; texture name "<set name>.png", texture file "<file base name>.png" next to the XML.
    CHECK(cube.m_Name == QStringLiteral("3_A CUBE"));
    CHECK(slab.m_Name == QStringLiteral("7_SLAB"));
    const auto textureFile = QFileInfo(path).absolutePath() + QStringLiteral("/trileset_valid.png");
    for (const auto* geometry : {&cube, &slab})
    {
        CHECK(geometry->m_Texture.m_TextureName == QStringLiteral("Fixture Set.png"));
        CHECK(geometry->m_Texture.m_TextureOrgFile == textureFile);
        CHECK(geometry->m_Indices == fez::Geometry::Indices{0, 1, 2});
        REQUIRE(geometry->m_Vertices.size() == 3);
        CHECK(geometry->m_Vertices[0].m_Position == Vec3f{-0.5f, -0.5f, 0.5f});
        CHECK(geometry->m_Opacity == 1.0f);
        CHECK(geometry->m_DoubleSided);
        CHECK_FALSE(geometry->m_IsPlane);
    }

    // Geometry comes from Trile/Geometry/ShaderInstancedIndexedPrimitives (parseGeometry: side 2 and 4 shift U).
    CHECK(cube.m_Vertices[0].m_TextureCoordinate == Vec2f{2.0f, 0.0f});
    CHECK(slab.m_Vertices[1].m_TextureCoordinate == Vec2f{4.125f, 0.0f});
}

TEST_CASE("parseTrileSet rejects a non-integer key as BadValue", "[parser]")
{
    const auto set = fez::parseTrileSet(fixture(QStringLiteral("trileset_invalid_key.xml")));
    REQUIRE_FALSE(set.has_value());
    CHECK(set.error().kind == ErrorKind::BadValue);
    CHECK(set.error().source == "TrileSet/Triles/TrileEntry[2]@key");
}

TEST_CASE("parseTrileSet writes nothing next to the trile set", "[parser]")
{
    QTemporaryDir dir;
    REQUIRE(dir.isValid());
    REQUIRE(QFile::copy(fixture(QStringLiteral("trileset_valid.xml")), dir.filePath(QStringLiteral("set.xml"))));

    const auto set = fez::parseTrileSet(dir.filePath(QStringLiteral("set.xml")));
    REQUIRE(set.has_value());
    CHECK(listTree(dir.path()).join(QLatin1Char('|')).toStdString() == "set.xml");
}

TEST_CASE("parseTrileSet keeps the first of two triles with the same key", "[parser]")
{
    QTemporaryDir dir;
    REQUIRE(dir.isValid());
    const auto path = writeFile(dir, QStringLiteral("dup.xml"), trileSet(trileEntry("1", "FIRST") + trileEntry("1", "SECOND")));

    const auto set = fez::parseTrileSet(path);
    REQUIRE(set.has_value());
    REQUIRE(set->m_Triles.size() == 1);
    CHECK(set->m_Triles.at(1).m_Name == QStringLiteral("1_FIRST"));
}

TEST_CASE("parseTrileSet accepts a set without trile entries", "[parser]")
{
    QTemporaryDir dir;
    REQUIRE(dir.isValid());
    const auto path = writeFile(dir, QStringLiteral("empty.xml"), trileSet(""));

    const auto set = fez::parseTrileSet(path);
    REQUIRE(set.has_value());
    CHECK(set->m_Name == QStringLiteral("inline"));
    CHECK(set->m_Triles.empty());
}

TEST_CASE("parseTrileSet visits every element after the first TrileEntry", "[parser]")
{
    // As before: after the first TrileEntry every following sibling element is parsed as an entry, whatever its name.
    QTemporaryDir dir;
    REQUIRE(dir.isValid());
    const auto path = writeFile(dir, QStringLiteral("other.xml"), trileSet(trileEntry("1", "ONE") + "<Other/>"));

    const auto set = fez::parseTrileSet(path);
    REQUIRE_FALSE(set.has_value());
    CHECK(set.error().kind == ErrorKind::MissingAttribute);
    CHECK(set.error().source == "TrileSet/Triles/Other@key");
}

TEST_CASE("parseTrileSet rejects structural errors", "[parser]")
{
    QTemporaryDir dir;
    REQUIRE(dir.isValid());

    struct Case
    {
        const char* label;
        QByteArray content;
        ErrorKind kind;
        std::string source;
    };

    const Case cases[] = {
        {"no TrileSet root", "<Other name=\"x\"><Triles/></Other>", ErrorKind::MissingElement, "TrileSet"},
        {"no set name", "<TrileSet><Triles/></TrileSet>", ErrorKind::MissingAttribute, "TrileSet@name"},
        {"no Triles", "<TrileSet name=\"x\"/>", ErrorKind::MissingElement, "TrileSet/Triles"},
        {"no key", trileSet("<TrileEntry><Trile name=\"a\"/></TrileEntry>"), ErrorKind::MissingAttribute,
         "TrileSet/Triles/TrileEntry@key"},
        {"no Trile", trileSet("<TrileEntry key=\"1\"/>"), ErrorKind::MissingElement, "TrileSet/Triles/TrileEntry/Trile"},
        {"no trile name", trileSet("<TrileEntry key=\"1\"><Trile><Geometry/></Trile></TrileEntry>"),
         ErrorKind::MissingAttribute, "TrileSet/Triles/TrileEntry/Trile@name"},
        {"no Geometry", trileSet("<TrileEntry key=\"1\"><Trile name=\"a\"/></TrileEntry>"), ErrorKind::MissingElement,
         "TrileSet/Triles/TrileEntry/Trile/Geometry"},
        {"no primitives", trileSet("<TrileEntry key=\"1\"><Trile name=\"a\"><Geometry/></Trile></TrileEntry>"),
         ErrorKind::MissingElement, "TrileSet/Triles/TrileEntry/Trile/Geometry/ShaderInstancedIndexedPrimitives"},
    };

    for (const auto& c : cases)
    {
        INFO(c.label);
        const auto path = writeFile(dir, QStringLiteral("case.xml"), c.content);
        const auto set = fez::parseTrileSet(path);
        REQUIRE_FALSE(set.has_value());
        CHECK(set.error().kind == c.kind);
        CHECK(set.error().source == c.source);
    }
}

TEST_CASE("parseTrileSet reports a missing file as FileNotFound", "[parser]")
{
    const auto path = fixture(QStringLiteral("trileset_does_not_exist.xml"));
    const auto set = fez::parseTrileSet(path);
    REQUIRE_FALSE(set.has_value());
    CHECK(set.error().kind == ErrorKind::FileNotFound);
    CHECK(set.error().source == path.toStdString());
}

TEST_CASE("parseTrileSet reports malformed XML as XmlSyntax", "[parser]")
{
    QTemporaryDir dir;
    REQUIRE(dir.isValid());
    const auto path = writeFile(dir, QStringLiteral("broken.xml"), "<TrileSet name=\"x\"><Triles></TrileSet>");

    const auto set = fez::parseTrileSet(path);
    REQUIRE_FALSE(set.has_value());
    CHECK(set.error().kind == ErrorKind::XmlSyntax);
}
