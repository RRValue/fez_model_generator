#include "error/Error.h"
#include "format/QStringFormatter.h"
#include "xml/Xml.h"

#include <QtCore/QByteArray>
#include <QtCore/QFile>
#include <QtCore/QString>
#include <QtCore/QTemporaryDir>
#include <QtXml/QDomDocument>
#include <QtXml/QDomElement>

#include <catch2/catch_test_macros.hpp>

#include <format>
#include <string>
#include <vector>

using fez::ErrorKind;

namespace
{
    // Parses an in-memory XML snippet; the tests below only use well-formed snippets with it.
    QDomDocument parse(const char* xml)
    {
        auto document = fez::parseXml(QByteArray(xml), QStringLiteral("test.xml"));
        REQUIRE(document.has_value());
        return *document;
    }

    // The FEZ level layout used for element-path tests: Level/ArtObjects/Entry[n]/ArtObjectInstance.
    constexpr const char* kLevelXml = R"(<Level name="L">
  <ArtObjects>
    <Entry key="0"><ArtObjectInstance name="a"/></Entry>
    <Entry key="1"><ArtObjectInstance name="b"/></Entry>
    <Entry key="2"><ArtObjectInstance/></Entry>
  </ArtObjects>
</Level>)";

    QDomElement thirdArtObjectInstance(const QDomDocument& document)
    {
        auto entry = document.documentElement().firstChildElement("ArtObjects").firstChildElement("Entry");
        entry = entry.nextSiblingElement("Entry").nextSiblingElement("Entry");
        return entry.firstChildElement("ArtObjectInstance");
    }

    QString writeTempFile(const QTemporaryDir& dir, const QString& name, const QByteArray& content)
    {
        const auto path = dir.filePath(name);
        QFile file(path);
        REQUIRE(file.open(QIODevice::WriteOnly));
        REQUIRE(file.write(content) == content.size());
        return path;
    }
} // namespace

// --- Error and formatting ---------------------------------------------------------------------------------------

TEST_CASE("Error carries kind, message and source", "[xml][error]")
{
    const fez::Error error{ErrorKind::MissingAttribute, "missing attribute 'name'", "Level@name"};
    CHECK(error.kind == ErrorKind::MissingAttribute);
    CHECK(error.message == "missing attribute 'name'");
    CHECK(error.source == "Level@name");
}

TEST_CASE("Every ErrorKind has a name", "[xml][error]")
{
    CHECK(fez::toString(ErrorKind::FileNotFound) == "FileNotFound");
    CHECK(fez::toString(ErrorKind::XmlSyntax) == "XmlSyntax");
    CHECK(fez::toString(ErrorKind::MissingElement) == "MissingElement");
    CHECK(fez::toString(ErrorKind::MissingAttribute) == "MissingAttribute");
    CHECK(fez::toString(ErrorKind::BadValue) == "BadValue");
    CHECK(fez::toString(ErrorKind::Export) == "Export");
    CHECK(fez::toString(ErrorKind::Io) == "Io");
}

TEST_CASE("std::format accepts QString as UTF-8 with string format specs", "[xml][format]")
{
    CHECK(std::format("{}", QString()) == "");
    CHECK(std::format("<{}>", QStringLiteral("abc")) == "<abc>");
    CHECK(std::format("{}", QString::fromUtf8("\xC3\xA4\xE2\x82\xAC")) == "\xC3\xA4\xE2\x82\xAC");
    CHECK(std::format("[{:>5}]", QStringLiteral("ab")) == "[   ab]");
}

// --- loadXml / parseXml -----------------------------------------------------------------------------------------

TEST_CASE("loadXml reads a well-formed file", "[xml]")
{
    QTemporaryDir dir;
    REQUIRE(dir.isValid());
    const auto path = writeTempFile(dir, "ok.xml", R"(<Level name="L"><ArtObjects/></Level>)");

    const auto document = fez::loadXml(path);
    REQUIRE(document.has_value());
    CHECK(document->documentElement().tagName() == "Level");
    CHECK(document->documentElement().attribute("name") == "L");
}

TEST_CASE("loadXml on a missing file returns FileNotFound with the path", "[xml]")
{
    QTemporaryDir dir;
    REQUIRE(dir.isValid());
    const auto path = dir.filePath("does_not_exist.xml");

    const auto document = fez::loadXml(path);
    REQUIRE_FALSE(document.has_value());
    CHECK(document.error().kind == ErrorKind::FileNotFound);
    CHECK(document.error().source == path.toStdString());
}

TEST_CASE("loadXml on a directory returns Io", "[xml]")
{
    QTemporaryDir dir;
    REQUIRE(dir.isValid());

    const auto document = fez::loadXml(dir.path());
    REQUIRE_FALSE(document.has_value());
    CHECK(document.error().kind == ErrorKind::Io);
    CHECK(document.error().source == dir.path().toStdString());
}

TEST_CASE("loadXml on a malformed file returns XmlSyntax with line and column", "[xml]")
{
    QTemporaryDir dir;
    REQUIRE(dir.isValid());
    // The closing tag on line 3 does not match <B>.
    const auto path = writeTempFile(dir, "bad.xml", "<A>\n  <B>\n  </C>\n</A>\n");

    const auto document = fez::loadXml(path);
    REQUIRE_FALSE(document.has_value());
    const auto& error = document.error();
    CHECK(error.kind == ErrorKind::XmlSyntax);
    CHECK_FALSE(error.message.empty());
    // source = "<path>:<line>:<column>"
    const auto prefix = path.toStdString() + ":3:";
    REQUIRE(error.source.size() > prefix.size());
    CHECK(error.source.starts_with(prefix));
    const auto column = std::stoi(error.source.substr(prefix.size()));
    CHECK(column > 0);
}

TEST_CASE("parseXml reports syntax errors with the given source name", "[xml]")
{
    const auto document = fez::parseXml(QByteArray("<A attr=1/>"), QStringLiteral("snippet"));
    REQUIRE_FALSE(document.has_value());
    CHECK(document.error().kind == ErrorKind::XmlSyntax);
    CHECK(document.error().source.starts_with("snippet:1:"));
}

// --- elementPath ------------------------------------------------------------------------------------------------

TEST_CASE("elementPath names every ancestor and indexes repeated siblings from 1", "[xml]")
{
    const auto document = parse(kLevelXml);
    const auto instance = thirdArtObjectInstance(document);
    REQUIRE_FALSE(instance.isNull());

    CHECK(fez::elementPath(instance) == "Level/ArtObjects/Entry[3]/ArtObjectInstance");
    CHECK(fez::elementPath(document.documentElement()) == "Level");
    CHECK(fez::elementPath(QDomElement()) == "");
}

TEST_CASE("elementPath does not index an element without same-named siblings", "[xml]")
{
    const auto document = parse("<Root><A/><B><C/></B><A/></Root>");
    const auto c = document.documentElement().firstChildElement("B").firstChildElement("C");
    CHECK(fez::elementPath(c) == "Root/B/C");
    const auto second_a = document.documentElement().lastChildElement("A");
    CHECK(fez::elementPath(second_a) == "Root/A[2]");
}

// --- childElement / childElements -------------------------------------------------------------------------------

TEST_CASE("childElement returns the first child with the given name", "[xml]")
{
    const auto document = parse("<Root><A id='1'/><B/><A id='2'/></Root>");
    const auto child = fez::childElement(document.documentElement(), "A");
    REQUIRE(child.has_value());
    CHECK(child->attribute("id") == "1");
}

TEST_CASE("childElement on a missing child returns MissingElement with the child's path", "[xml]")
{
    const auto document = parse(kLevelXml);
    const auto instance = thirdArtObjectInstance(document);

    const auto child = fez::childElement(instance, "Position");
    REQUIRE_FALSE(child.has_value());
    CHECK(child.error().kind == ErrorKind::MissingElement);
    CHECK(child.error().source == "Level/ArtObjects/Entry[3]/ArtObjectInstance/Position");
}

TEST_CASE("childElements iterates the direct children with the given name in document order", "[xml]")
{
    const auto document = parse("<Root><A id='1'><A id='nested'/></A><B id='b'/><A id='2'/><A id='3'/></Root>");

    std::vector<QString> ids;
    for(const auto& element : fez::childElements(document.documentElement(), "A"))
        ids.push_back(element.attribute("id"));
    CHECK(ids == std::vector<QString>{"1", "2", "3"});
}

TEST_CASE("childElements with an empty name iterates all direct child elements", "[xml]")
{
    const auto document = parse("<Root><A id='1'/>text<!-- c --><B id='b'/></Root>");

    std::vector<QString> ids;
    for(const auto& element : fez::childElements(document.documentElement()))
        ids.push_back(element.attribute("id"));
    CHECK(ids == std::vector<QString>{"1", "b"});
}

TEST_CASE("childElements is empty for missing children and null elements", "[xml]")
{
    const auto document = parse("<Root><B/></Root>");
    CHECK(fez::childElements(document.documentElement(), "A").begin() ==
          fez::childElements(document.documentElement(), "A").end());
    CHECK(fez::childElements(QDomElement()).begin() == fez::childElements(QDomElement()).end());

    auto count = 0;
    for([[maybe_unused]] const auto& element : fez::childElements(document.documentElement(), "A"))
        ++count;
    CHECK(count == 0);
}

// --- attribute<T> -----------------------------------------------------------------------------------------------

TEST_CASE("attribute<T> reads QString, float, int and unsigned", "[xml]")
{
    const auto document = parse(R"(<E s="text" f="-1.5" i="-42" u="7"/>)");
    const auto element = document.documentElement();

    CHECK(fez::attribute<QString>(element, "s") == QString("text"));
    CHECK(fez::attribute<float>(element, "f") == -1.5f);
    CHECK(fez::attribute<int>(element, "i") == -42);
    CHECK(fez::attribute<unsigned>(element, "u") == 7u);
}

TEST_CASE("attribute<QString> accepts an empty attribute", "[xml]")
{
    const auto document = parse(R"(<E s=""/>)");
    CHECK(fez::attribute<QString>(document.documentElement(), "s") == QString());
}

TEST_CASE("attribute<T> on a missing attribute returns MissingAttribute with element path and name", "[xml]")
{
    const auto document = parse(kLevelXml);
    const auto instance = thirdArtObjectInstance(document);

    const auto check_missing = [](const auto& result) {
        REQUIRE_FALSE(result.has_value());
        CHECK(result.error().kind == ErrorKind::MissingAttribute);
        CHECK(result.error().source == "Level/ArtObjects/Entry[3]/ArtObjectInstance@name");
    };
    check_missing(fez::attribute<QString>(instance, "name"));
    check_missing(fez::attribute<float>(instance, "name"));
    check_missing(fez::attribute<int>(instance, "name"));
    check_missing(fez::attribute<unsigned>(instance, "name"));
}

TEST_CASE("attribute<T> on a malformed number returns BadValue with element path and name", "[xml]")
{
    const auto document = parse(R"(<Root><E f="1.5x" i="1.5" u="-1" empty=""/></Root>)");
    const auto element = document.documentElement().firstChildElement("E");

    const auto check_bad = [](const auto& result, const std::string& source) {
        REQUIRE_FALSE(result.has_value());
        CHECK(result.error().kind == ErrorKind::BadValue);
        CHECK(result.error().source == source);
    };
    check_bad(fez::attribute<float>(element, "f"), "Root/E@f");
    check_bad(fez::attribute<int>(element, "i"), "Root/E@i");
    check_bad(fez::attribute<unsigned>(element, "u"), "Root/E@u");
    check_bad(fez::attribute<float>(element, "empty"), "Root/E@empty");

    const auto result = fez::attribute<float>(element, "f");
    CHECK(result.error().message.find("1.5x") != std::string::npos);
}

// --- readBool ---------------------------------------------------------------------------------------------------

TEST_CASE("readBool accepts true and false case-insensitively", "[xml]")
{
    const auto document = parse(R"(<E a="True" b="FALSE" c="true" d="false" e="tRuE"/>)");
    const auto element = document.documentElement();

    CHECK(fez::readBool(element, "a") == true);
    CHECK(fez::readBool(element, "b") == false);
    CHECK(fez::readBool(element, "c") == true);
    CHECK(fez::readBool(element, "d") == false);
    CHECK(fez::readBool(element, "e") == true);
}

TEST_CASE("readBool on a missing attribute returns MissingAttribute", "[xml]")
{
    const auto document = parse(R"(<Root><Plane/></Root>)");
    const auto result = fez::readBool(document.documentElement().firstChildElement("Plane"), "animated");
    REQUIRE_FALSE(result.has_value());
    CHECK(result.error().kind == ErrorKind::MissingAttribute);
    CHECK(result.error().source == "Root/Plane@animated");
}

TEST_CASE("readBool on anything but true/false returns BadValue", "[xml]")
{
    const auto document = parse(R"(<Root><Plane a="1" b="yes" c=""/></Root>)");
    const auto plane = document.documentElement().firstChildElement("Plane");
    for(const auto* name : {"a", "b", "c"})
    {
        const auto result = fez::readBool(plane, name);
        REQUIRE_FALSE(result.has_value());
        CHECK(result.error().kind == ErrorKind::BadValue);
        CHECK(result.error().source == std::string("Root/Plane@") + name);
    }
}

// --- readVector2 / readVector3 / readQuaternion -----------------------------------------------------------------

TEST_CASE("readVector2 reads x and y", "[xml]")
{
    const auto document = parse(R"(<Vector2 x="0.25" y="-3"/>)");
    const auto result = fez::readVector2(document.documentElement());
    REQUIRE(result.has_value());
    CHECK(*result == fez::Vec2f(0.25f, -3.0f));
}

TEST_CASE("readVector3 reads x, y and z", "[xml]")
{
    const auto document = parse(R"(<Vector3 x="1" y="2.5" z="-0.125"/>)");
    const auto result = fez::readVector3(document.documentElement());
    REQUIRE(result.has_value());
    CHECK(*result == fez::Vec3f(1.0f, 2.5f, -0.125f));
}

TEST_CASE("readQuaternion reads w, x, y and z", "[xml]")
{
    const auto document = parse(R"(<Quaternion x="0.1" y="0.2" z="0.3" w="0.9"/>)");
    const auto result = fez::readQuaternion(document.documentElement());
    REQUIRE(result.has_value());
    CHECK(result->w() == 0.9f);
    CHECK(result->x() == 0.1f);
    CHECK(result->y() == 0.2f);
    CHECK(result->z() == 0.3f);
    // Same construction as the parsers today: QuaternionF{w, x, y, z}.
    CHECK(result->coeffs() == fez::QuaternionF{0.9f, 0.1f, 0.2f, 0.3f}.coeffs());
}

TEST_CASE("readVector2/3 and readQuaternion on a missing component return MissingAttribute with its path", "[xml]")
{
    const auto document = parse(R"(<Root><Position><Vector3 x="1" z="3"/></Position>)"
                                R"(<Size><Vector2 x="1"/></Size><Rotation><Quaternion x="0" y="0" z="0"/></Rotation></Root>)");
    const auto root = document.documentElement();

    const auto v3 = fez::readVector3(root.firstChildElement("Position").firstChildElement("Vector3"));
    REQUIRE_FALSE(v3.has_value());
    CHECK(v3.error().kind == ErrorKind::MissingAttribute);
    CHECK(v3.error().source == "Root/Position/Vector3@y");

    const auto v2 = fez::readVector2(root.firstChildElement("Size").firstChildElement("Vector2"));
    REQUIRE_FALSE(v2.has_value());
    CHECK(v2.error().kind == ErrorKind::MissingAttribute);
    CHECK(v2.error().source == "Root/Size/Vector2@y");

    const auto q = fez::readQuaternion(root.firstChildElement("Rotation").firstChildElement("Quaternion"));
    REQUIRE_FALSE(q.has_value());
    CHECK(q.error().kind == ErrorKind::MissingAttribute);
    CHECK(q.error().source == "Root/Rotation/Quaternion@w");
}

TEST_CASE("readVector2/3 and readQuaternion on a malformed component return BadValue with its path", "[xml]")
{
    const auto document = parse(R"(<Root><Vector3 x="1" y="2" z="three"/><Vector2 x="nan?" y="1"/>)"
                                R"(<Quaternion w="1" x="0" y="," z="0"/></Root>)");
    const auto root = document.documentElement();

    const auto v3 = fez::readVector3(root.firstChildElement("Vector3"));
    REQUIRE_FALSE(v3.has_value());
    CHECK(v3.error().kind == ErrorKind::BadValue);
    CHECK(v3.error().source == "Root/Vector3@z");

    const auto v2 = fez::readVector2(root.firstChildElement("Vector2"));
    REQUIRE_FALSE(v2.has_value());
    CHECK(v2.error().kind == ErrorKind::BadValue);
    CHECK(v2.error().source == "Root/Vector2@x");

    const auto q = fez::readQuaternion(root.firstChildElement("Quaternion"));
    REQUIRE_FALSE(q.has_value());
    CHECK(q.error().kind == ErrorKind::BadValue);
    CHECK(q.error().source == "Root/Quaternion@y");
}

TEST_CASE("readVector3 composes with childElement for a missing Vector3 element", "[xml]")
{
    const auto document = parse(kLevelXml);
    const auto instance = thirdArtObjectInstance(document);

    const auto result = fez::childElement(instance, "Position")
                            .and_then([](const QDomElement& position) { return fez::childElement(position, "Vector3"); })
                            .and_then([](const QDomElement& vector3) { return fez::readVector3(vector3); });
    REQUIRE_FALSE(result.has_value());
    CHECK(result.error().kind == ErrorKind::MissingElement);
    CHECK(result.error().source == "Level/ArtObjects/Entry[3]/ArtObjectInstance/Position");
}
