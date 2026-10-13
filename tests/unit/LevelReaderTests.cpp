#include "parser/level/LevelReader.h"

#include "support/TestData.h"
#include "xml/Xml.h"

#include <catch2/catch_test_macros.hpp>

#include <QtCore/QByteArray>
#include <QtCore/QFile>
#include <QtCore/QTemporaryDir>

#include <string>
#include <vector>

using fez::ErrorKind;
using fez::Vec3f;

namespace
{

QString fixture(const QString& name)
{
    return fez::testing::testDataPath(QStringLiteral("parser/") + name);
}

const QByteArray validTrile = "<Entry><TrileEmplacement x=\"1\" y=\"2\" z=\"3\"/>"
                              "<TrileInstance trileId=\"4\" orientation=\"1\">"
                              "<Position><Vector3 x=\"1\" y=\"2\" z=\"3\"/></Position></TrileInstance></Entry>";

const QByteArray validArtObject = "<Entry><ArtObjectInstance name=\"ao\">"
                                  "<Position><Vector3 x=\"1\" y=\"2\" z=\"3\"/></Position>"
                                  "<Rotation><Quaternion x=\"0\" y=\"0\" z=\"0\" w=\"1\"/></Rotation>"
                                  "<Scale><Vector3 x=\"1\" y=\"1\" z=\"1\"/></Scale></ArtObjectInstance></Entry>";

QByteArray level(const QByteArray& triles, const QByteArray& artObjects)
{
    return "<Level name=\"L\" trileSetName=\"S\"><Triles>" + triles + "</Triles><ArtObjects>" + artObjects
           + "</ArtObjects></Level>";
}

std::expected<fez::LevelDescription, fez::Error> readInline(const QByteArray& xml)
{
    const auto document = fez::parseXml(xml, QStringLiteral("inline.xml"));
    REQUIRE(document.has_value());
    return fez::readLevel(*document);
}

struct FailureCase
{
    const char* what;
    QByteArray xml;
    ErrorKind kind;
    std::string source;
};

void checkFailures(const std::vector<FailureCase>& cases)
{
    for (const auto& failure : cases)
    {
        INFO(failure.what);
        const auto description = readInline(failure.xml);
        REQUIRE_FALSE(description.has_value());
        CHECK(description.error().kind == failure.kind);
        CHECK(description.error().source == failure.source);
    }
}

} // namespace

TEST_CASE("parseLevel reads the header", "[level]")
{
    const auto description = fez::parseLevel(fixture(QStringLiteral("level_valid.xml")));
    REQUIRE(description.has_value());
    CHECK(description->m_Name == QStringLiteral("FIXTURE LEVEL"));
    CHECK(description->m_TrileSetName == QStringLiteral("Fixture Set"));
}

TEST_CASE("parseLevel reads the Triles section", "[level]")
{
    const auto description = fez::parseLevel(fixture(QStringLiteral("level_valid.xml")));
    REQUIRE(description.has_value());

    const auto& triles = description->m_TrileEmplacements;
    REQUIRE(triles.size() == 3);

    CHECK(triles[0].m_Emplacement == Vec3f{1.0f, 2.0f, 3.0f});
    CHECK(triles[0].m_Id == 7);
    CHECK(triles[0].m_Orientation == 2u);
    CHECK(triles[0].m_Position == Vec3f{1.5f, 2.25f, -3.0f});

    // Id -1 is kept here (the level parser skips it when looking up geometries).
    CHECK(triles[1].m_Id == -1);
    CHECK(triles[1].m_Orientation == 0u);

    // trileId and orientation are read as floats and truncated (as before; Task 30 changes this).
    CHECK(triles[2].m_Emplacement == Vec3f{-4.0f, 5.0f, 6.0f});
    CHECK(triles[2].m_Id == 3);
    CHECK(triles[2].m_Orientation == 1u);
    CHECK(triles[2].m_Position == Vec3f{-4.0f, 5.0f, 6.5f});
}

TEST_CASE("parseLevel reads the ArtObjects section with quaternions in w, x, y, z order", "[level]")
{
    const auto description = fez::parseLevel(fixture(QStringLiteral("level_valid.xml")));
    REQUIRE(description.has_value());

    const auto& artObjects = description->m_ArtObjects;
    REQUIRE(artObjects.size() == 2);

    CHECK(artObjects[0].m_Name == QStringLiteral("fixture_ao"));
    CHECK(artObjects[0].m_Position == Vec3f{1.5f, 1.5f, 0.5f});
    CHECK(artObjects[0].m_Rotation.w() == 0.9f);
    CHECK(artObjects[0].m_Rotation.x() == 0.1f);
    CHECK(artObjects[0].m_Rotation.y() == 0.2f);
    CHECK(artObjects[0].m_Rotation.z() == 0.3f);
    CHECK(artObjects[0].m_Scale == Vec3f{1.0f, 2.0f, 0.5f});

    CHECK(artObjects[1].m_Name == QStringLiteral("second ao"));
    CHECK(artObjects[1].m_Position == Vec3f{-2.0f, 0.0f, 8.0f});
    CHECK(artObjects[1].m_Rotation.coeffs() == fez::QuaternionF::Identity().coeffs());
    CHECK(artObjects[1].m_Scale == Vec3f{1.0f, 1.0f, 1.0f});
}

TEST_CASE("parseLevel rejects a level without trileSetName", "[level]")
{
    const auto description = fez::parseLevel(fixture(QStringLiteral("level_missing_trileset_name.xml")));
    REQUIRE_FALSE(description.has_value());
    CHECK(description.error().kind == ErrorKind::MissingAttribute);
    CHECK(description.error().source == "Level@trileSetName");
}

TEST_CASE("parseLevel reports a missing file and malformed XML", "[level]")
{
    const auto missing = fez::parseLevel(fixture(QStringLiteral("no_such_level.xml")));
    REQUIRE_FALSE(missing.has_value());
    CHECK(missing.error().kind == ErrorKind::FileNotFound);

    QTemporaryDir dir;
    REQUIRE(dir.isValid());
    const auto path = dir.filePath(QStringLiteral("broken.xml"));
    {
        QFile file(path);
        REQUIRE(file.open(QIODevice::WriteOnly));
        file.write("<Level name=\"L\" trileSetName=\"S\"><Triles></Level>");
    }

    const auto malformed = fez::parseLevel(path);
    REQUIRE_FALSE(malformed.has_value());
    CHECK(malformed.error().kind == ErrorKind::XmlSyntax);
}

TEST_CASE("readLevel rejects a document without Level element or header attributes", "[level]")
{
    checkFailures({
        {"no Level element", "<NotALevel name=\"L\" trileSetName=\"S\"><Triles/><ArtObjects/></NotALevel>",
         ErrorKind::MissingElement, "Level"},
        {"no name", "<Level trileSetName=\"S\"><Triles/><ArtObjects/></Level>", ErrorKind::MissingAttribute,
         "Level@name"},
        {"no trileSetName", "<Level name=\"L\"><Triles/><ArtObjects/></Level>", ErrorKind::MissingAttribute,
         "Level@trileSetName"},
    });
}

TEST_CASE("readLevel accepts empty Triles and ArtObjects sections", "[level]")
{
    const auto description = readInline(level("", ""));
    REQUIRE(description.has_value());
    CHECK(description->m_Name == QStringLiteral("L"));
    CHECK(description->m_TrileSetName == QStringLiteral("S"));
    CHECK(description->m_TrileEmplacements.empty());
    CHECK(description->m_ArtObjects.empty());
}

TEST_CASE("readLevel rejects a level without Triles or ArtObjects element", "[level]")
{
    checkFailures({
        {"no Triles", "<Level name=\"L\" trileSetName=\"S\"><ArtObjects>" + validArtObject + "</ArtObjects></Level>",
         ErrorKind::MissingElement, "Level/Triles"},
        {"no ArtObjects", "<Level name=\"L\" trileSetName=\"S\"><Triles>" + validTrile + "</Triles></Level>",
         ErrorKind::MissingElement, "Level/ArtObjects"},
    });
}

TEST_CASE("readLevel fails the whole level for a broken trile entry", "[level]")
{
    const auto broken = [](const QByteArray& entry) { return level(validTrile + entry, validArtObject); };
    const QByteArray position = "<Position><Vector3 x=\"1\" y=\"2\" z=\"3\"/></Position>";
    const QByteArray emplacement = "<TrileEmplacement x=\"1\" y=\"2\" z=\"3\"/>";

    checkFailures({
        {"no TrileEmplacement",
         broken("<Entry><TrileInstance trileId=\"4\" orientation=\"1\">" + position + "</TrileInstance></Entry>"),
         ErrorKind::MissingElement, "Level/Triles/Entry[2]/TrileEmplacement"},
        {"no TrileInstance", broken("<Entry>" + emplacement + "</Entry>"), ErrorKind::MissingElement,
         "Level/Triles/Entry[2]/TrileInstance"},
        {"no Position",
         broken("<Entry>" + emplacement + "<TrileInstance trileId=\"4\" orientation=\"1\"/></Entry>"),
         ErrorKind::MissingElement, "Level/Triles/Entry[2]/TrileInstance/Position"},
        {"no Vector3",
         broken("<Entry>" + emplacement + "<TrileInstance trileId=\"4\" orientation=\"1\"><Position/></TrileInstance></Entry>"),
         ErrorKind::MissingElement, "Level/Triles/Entry[2]/TrileInstance/Position/Vector3"},
        {"emplacement x missing",
         broken("<Entry><TrileEmplacement y=\"2\" z=\"3\"/><TrileInstance trileId=\"4\" orientation=\"1\">" + position
                + "</TrileInstance></Entry>"),
         ErrorKind::MissingAttribute, "Level/Triles/Entry[2]/TrileEmplacement@x"},
        {"emplacement z not a number",
         broken("<Entry><TrileEmplacement x=\"1\" y=\"2\" z=\"three\"/><TrileInstance trileId=\"4\" orientation=\"1\">"
                + position + "</TrileInstance></Entry>"),
         ErrorKind::BadValue, "Level/Triles/Entry[2]/TrileEmplacement@z"},
        {"trileId missing",
         broken("<Entry>" + emplacement + "<TrileInstance orientation=\"1\">" + position + "</TrileInstance></Entry>"),
         ErrorKind::MissingAttribute, "Level/Triles/Entry[2]/TrileInstance@trileId"},
        {"trileId not a number",
         broken("<Entry>" + emplacement + "<TrileInstance trileId=\"x\" orientation=\"1\">" + position
                + "</TrileInstance></Entry>"),
         ErrorKind::BadValue, "Level/Triles/Entry[2]/TrileInstance@trileId"},
        {"orientation missing",
         broken("<Entry>" + emplacement + "<TrileInstance trileId=\"4\">" + position + "</TrileInstance></Entry>"),
         ErrorKind::MissingAttribute, "Level/Triles/Entry[2]/TrileInstance@orientation"},
        {"orientation not a number",
         broken("<Entry>" + emplacement + "<TrileInstance trileId=\"4\" orientation=\"north\">" + position
                + "</TrileInstance></Entry>"),
         ErrorKind::BadValue, "Level/Triles/Entry[2]/TrileInstance@orientation"},
        {"position y missing",
         broken("<Entry>" + emplacement
                + "<TrileInstance trileId=\"4\" orientation=\"1\"><Position><Vector3 x=\"1\" z=\"3\"/></Position>"
                  "</TrileInstance></Entry>"),
         ErrorKind::MissingAttribute, "Level/Triles/Entry[2]/TrileInstance/Position/Vector3@y"},
        {"position x not a number",
         broken("<Entry>" + emplacement
                + "<TrileInstance trileId=\"4\" orientation=\"1\"><Position><Vector3 x=\"\" y=\"2\" z=\"3\"/></Position>"
                  "</TrileInstance></Entry>"),
         ErrorKind::BadValue, "Level/Triles/Entry[2]/TrileInstance/Position/Vector3@x"},
    });
}

TEST_CASE("readLevel fails the whole level for a broken art object entry", "[level]")
{
    const auto broken = [](const QByteArray& instance) {
        return level(validTrile, validArtObject + "<Entry>" + instance + "</Entry>");
    };
    const QByteArray position = "<Position><Vector3 x=\"1\" y=\"2\" z=\"3\"/></Position>";
    const QByteArray rotation = "<Rotation><Quaternion x=\"0\" y=\"0\" z=\"0\" w=\"1\"/></Rotation>";
    const QByteArray scale = "<Scale><Vector3 x=\"1\" y=\"1\" z=\"1\"/></Scale>";
    const auto instance = [](const QByteArray& content) {
        return "<ArtObjectInstance name=\"ao\">" + content + "</ArtObjectInstance>";
    };

    checkFailures({
        {"no ArtObjectInstance", broken(""), ErrorKind::MissingElement, "Level/ArtObjects/Entry[2]/ArtObjectInstance"},
        {"name missing", broken("<ArtObjectInstance>" + position + rotation + scale + "</ArtObjectInstance>"),
         ErrorKind::MissingAttribute, "Level/ArtObjects/Entry[2]/ArtObjectInstance@name"},
        {"no Position", broken(instance(rotation + scale)), ErrorKind::MissingElement,
         "Level/ArtObjects/Entry[2]/ArtObjectInstance/Position"},
        {"no Position/Vector3", broken(instance("<Position/>" + rotation + scale)), ErrorKind::MissingElement,
         "Level/ArtObjects/Entry[2]/ArtObjectInstance/Position/Vector3"},
        {"position z not a number",
         broken(instance("<Position><Vector3 x=\"1\" y=\"2\" z=\"z\"/></Position>" + rotation + scale)),
         ErrorKind::BadValue, "Level/ArtObjects/Entry[2]/ArtObjectInstance/Position/Vector3@z"},
        {"no Rotation", broken(instance(position + scale)), ErrorKind::MissingElement,
         "Level/ArtObjects/Entry[2]/ArtObjectInstance/Rotation"},
        {"no Rotation/Quaternion", broken(instance(position + "<Rotation/>" + scale)), ErrorKind::MissingElement,
         "Level/ArtObjects/Entry[2]/ArtObjectInstance/Rotation/Quaternion"},
        {"quaternion w missing",
         broken(instance(position + "<Rotation><Quaternion x=\"0\" y=\"0\" z=\"0\"/></Rotation>" + scale)),
         ErrorKind::MissingAttribute, "Level/ArtObjects/Entry[2]/ArtObjectInstance/Rotation/Quaternion@w"},
        {"quaternion y not a number",
         broken(instance(position + "<Rotation><Quaternion x=\"0\" y=\"y\" z=\"0\" w=\"1\"/></Rotation>" + scale)),
         ErrorKind::BadValue, "Level/ArtObjects/Entry[2]/ArtObjectInstance/Rotation/Quaternion@y"},
        {"no Scale", broken(instance(position + rotation)), ErrorKind::MissingElement,
         "Level/ArtObjects/Entry[2]/ArtObjectInstance/Scale"},
        {"no Scale/Vector3", broken(instance(position + rotation + "<Scale/>")), ErrorKind::MissingElement,
         "Level/ArtObjects/Entry[2]/ArtObjectInstance/Scale/Vector3"},
        {"scale x missing",
         broken(instance(position + rotation + "<Scale><Vector3 y=\"1\" z=\"1\"/></Scale>")),
         ErrorKind::MissingAttribute, "Level/ArtObjects/Entry[2]/ArtObjectInstance/Scale/Vector3@x"},
    });
}

TEST_CASE("readLevel reads entries from the first Entry on, whatever the following element names", "[level]")
{
    // Elements before the first Entry are skipped; every element after it is read as an entry (as before).
    const QByteArray otherTrile = "<Other><TrileEmplacement x=\"9\" y=\"9\" z=\"9\"/>"
                                  "<TrileInstance trileId=\"5\" orientation=\"3\">"
                                  "<Position><Vector3 x=\"9\" y=\"9\" z=\"9\"/></Position></TrileInstance></Other>";
    const QByteArray otherArtObject = "<Other><ArtObjectInstance name=\"other\">"
                                      "<Position><Vector3 x=\"1\" y=\"2\" z=\"3\"/></Position>"
                                      "<Rotation><Quaternion x=\"0\" y=\"0\" z=\"0\" w=\"1\"/></Rotation>"
                                      "<Scale><Vector3 x=\"1\" y=\"1\" z=\"1\"/></Scale></ArtObjectInstance></Other>";

    const auto description = readInline(level("<Skipped/>" + validTrile + otherTrile, "<Skipped/>" + validArtObject + otherArtObject));
    REQUIRE(description.has_value());

    REQUIRE(description->m_TrileEmplacements.size() == 2);
    CHECK(description->m_TrileEmplacements[1].m_Id == 5);
    CHECK(description->m_TrileEmplacements[1].m_Emplacement == Vec3f{9.0f, 9.0f, 9.0f});

    REQUIRE(description->m_ArtObjects.size() == 2);
    CHECK(description->m_ArtObjects[1].m_Name == QStringLiteral("other"));

    // A following element that is not a valid entry fails the level.
    checkFailures({
        {"trailing non-entry in Triles", level(validTrile + "<Comment/>", validArtObject), ErrorKind::MissingElement,
         "Level/Triles/Comment/TrileEmplacement"},
        {"trailing non-entry in ArtObjects", level(validTrile, validArtObject + "<Comment/>"),
         ErrorKind::MissingElement, "Level/ArtObjects/Comment/ArtObjectInstance"},
    });
}

TEST_CASE("readLevel checks the Triles section before the ArtObjects section", "[level]")
{
    const auto description = readInline(level("<Entry/>", "<Entry/>"));
    REQUIRE_FALSE(description.has_value());
    CHECK(description.error().source == "Level/Triles/Entry/TrileEmplacement");
}
