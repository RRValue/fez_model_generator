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

const QByteArray validPlane = "<Entry><BackgroundPlane textureName=\"plane\" animated=\"false\" doubleSided=\"true\" opacity=\"1\">"
                              "<Position><Vector3 x=\"1\" y=\"2\" z=\"3\"/></Position>"
                              "<Rotation><Quaternion x=\"0\" y=\"0\" z=\"0\" w=\"1\"/></Rotation>"
                              "<Scale><Vector3 x=\"1\" y=\"1\" z=\"1\"/></Scale></BackgroundPlane></Entry>";

const QByteArray validNpc = "<Entry><NpcInstance name=\"npc\">"
                            "<Position><Vector3 x=\"1\" y=\"2\" z=\"3\"/></Position>"
                            "<Actions><Action key=\"Idle\"><NpcActionContent animationName=\"idle\"/></Action></Actions>"
                            "</NpcInstance></Entry>";

QByteArray level(const QByteArray& triles, const QByteArray& artObjects, const QByteArray& planes = {},
                 const QByteArray& npcs = {})
{
    return "<Level name=\"L\" trileSetName=\"S\"><Triles>" + triles + "</Triles><ArtObjects>" + artObjects
           + "</ArtObjects><BackgroundPlanes>" + planes + "</BackgroundPlanes><NonplayerCharacters>" + npcs
           + "</NonplayerCharacters></Level>";
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
    CHECK(triles[0].m_Orientation == 2);
    CHECK(triles[0].m_Position == Vec3f{1.5f, 2.25f, -3.0f});

    // Id -1 is kept here (the level parser skips it when looking up geometries).
    CHECK(triles[1].m_Id == -1);
    CHECK(triles[1].m_Orientation == 0);

    CHECK(triles[2].m_Emplacement == Vec3f{-4.0f, 5.0f, 6.0f});
    CHECK(triles[2].m_Id == 3);
    CHECK(triles[2].m_Orientation == 1);
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

TEST_CASE("readLevel accepts empty sections", "[level]")
{
    const auto description = readInline(level("", ""));
    REQUIRE(description.has_value());
    CHECK(description->m_Name == QStringLiteral("L"));
    CHECK(description->m_TrileSetName == QStringLiteral("S"));
    CHECK(description->m_TrileEmplacements.empty());
    CHECK(description->m_ArtObjects.empty());
    CHECK(description->m_BackgroundPlanes.empty());
    CHECK(description->m_Npcs.empty());
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
        {"trileId not an integer",
         broken("<Entry>" + emplacement + "<TrileInstance trileId=\"1.5\" orientation=\"1\">" + position
                + "</TrileInstance></Entry>"),
         ErrorKind::BadValue, "Level/Triles/Entry[2]/TrileInstance@trileId"},
        {"orientation missing",
         broken("<Entry>" + emplacement + "<TrileInstance trileId=\"4\">" + position + "</TrileInstance></Entry>"),
         ErrorKind::MissingAttribute, "Level/Triles/Entry[2]/TrileInstance@orientation"},
        {"orientation not a number",
         broken("<Entry>" + emplacement + "<TrileInstance trileId=\"4\" orientation=\"north\">" + position
                + "</TrileInstance></Entry>"),
         ErrorKind::BadValue, "Level/Triles/Entry[2]/TrileInstance@orientation"},
        {"orientation not an integer",
         broken("<Entry>" + emplacement + "<TrileInstance trileId=\"4\" orientation=\"1.5\">" + position
                + "</TrileInstance></Entry>"),
         ErrorKind::BadValue, "Level/Triles/Entry[2]/TrileInstance@orientation"},
        {"orientation 4",
         broken("<Entry>" + emplacement + "<TrileInstance trileId=\"4\" orientation=\"4\">" + position
                + "</TrileInstance></Entry>"),
         ErrorKind::BadValue, "Level/Triles/Entry[2]/TrileInstance@orientation"},
        {"orientation -1",
         broken("<Entry>" + emplacement + "<TrileInstance trileId=\"4\" orientation=\"-1\">" + position
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

TEST_CASE("parseLevel reads the BackgroundPlanes section", "[level]")
{
    const auto description = fez::parseLevel(fixture(QStringLiteral("level_valid.xml")));
    REQUIRE(description.has_value());

    const auto& planes = description->m_BackgroundPlanes;
    REQUIRE(planes.size() == 2);

    CHECK(planes[0].m_TextureName == QStringLiteral("fixture_plane"));
    CHECK_FALSE(planes[0].m_Animated);
    CHECK(planes[0].m_Opacity == 1.0f);
    CHECK(planes[0].m_DoubleSided);
    CHECK(planes[0].m_Position == Vec3f{0.5f, 1.5f, -0.5f});
    CHECK(planes[0].m_Rotation.coeffs() == fez::QuaternionF::Identity().coeffs());
    CHECK(planes[0].m_Scale == Vec3f{1.0f, 1.0f, 1.0f});

    // Backslashes in the texture name become slashes; booleans are case-insensitive.
    CHECK(planes[1].m_TextureName == QStringLiteral("sub/water_anim"));
    CHECK(planes[1].m_Animated);
    CHECK(planes[1].m_Opacity == 0.5f);
    CHECK_FALSE(planes[1].m_DoubleSided);
    CHECK(planes[1].m_Position == Vec3f{-1.0f, 2.0f, 3.5f});
    CHECK(planes[1].m_Rotation.w() == 0.9f);
    CHECK(planes[1].m_Rotation.x() == 0.1f);
    CHECK(planes[1].m_Rotation.y() == 0.2f);
    CHECK(planes[1].m_Rotation.z() == 0.3f);
    CHECK(planes[1].m_Scale == Vec3f{2.0f, 0.5f, 1.0f});
}

TEST_CASE("parseLevel reads the NonplayerCharacters section with all actions in order", "[level]")
{
    const auto description = fez::parseLevel(fixture(QStringLiteral("level_valid.xml")));
    REQUIRE(description.has_value());

    const auto& npcs = description->m_Npcs;
    REQUIRE(npcs.size() == 1);

    CHECK(npcs[0].m_Name == QStringLiteral("fixture_npc"));
    CHECK(npcs[0].m_Position == Vec3f{0.0f, 1.5f, 1.0f});

    // The first action is recorded first (it decides the NPC texture).
    REQUIRE(npcs[0].m_Actions.size() == 2);
    CHECK(npcs[0].m_Actions[0].first == QStringLiteral("Idle"));
    CHECK(npcs[0].m_Actions[0].second == QStringLiteral("idle"));
    CHECK(npcs[0].m_Actions[1].first == QStringLiteral("Walk"));
    CHECK(npcs[0].m_Actions[1].second == QStringLiteral("walk"));
}

TEST_CASE("readLevel replaces every backslash in a texture name and keeps an NPC name as written", "[level]")
{
    const QByteArray plane = "<Entry><BackgroundPlane textureName=\"a\\b\\c\" animated=\"False\" doubleSided=\"TRUE\" opacity=\"0.25\">"
                             "<Position><Vector3 x=\"1\" y=\"2\" z=\"3\"/></Position>"
                             "<Rotation><Quaternion x=\"0\" y=\"0\" z=\"0\" w=\"1\"/></Rotation>"
                             "<Scale><Vector3 x=\"1\" y=\"1\" z=\"1\"/></Scale></BackgroundPlane></Entry>";
    const QByteArray npc = "<Entry><NpcInstance name=\"some\\npc\">"
                           "<Position><Vector3 x=\"1\" y=\"2\" z=\"3\"/></Position>"
                           "<Actions><Action key=\"Talk\"><NpcActionContent animationName=\"talk\"/></Action></Actions>"
                           "</NpcInstance></Entry>";

    const auto description = readInline(level(validTrile, validArtObject, validPlane + plane, npc));
    REQUIRE(description.has_value());

    REQUIRE(description->m_BackgroundPlanes.size() == 2);
    CHECK(description->m_BackgroundPlanes[1].m_TextureName == QStringLiteral("a/b/c"));
    CHECK_FALSE(description->m_BackgroundPlanes[1].m_Animated);
    CHECK(description->m_BackgroundPlanes[1].m_DoubleSided);
    CHECK(description->m_BackgroundPlanes[1].m_Opacity == 0.25f);

    // The NPC name is not rewritten (as before).
    REQUIRE(description->m_Npcs.size() == 1);
    CHECK(description->m_Npcs[0].m_Name == QStringLiteral("some\\npc"));
}

TEST_CASE("readLevel rejects a level without BackgroundPlanes or NonplayerCharacters element", "[level]")
{
    const QByteArray head = "<Level name=\"L\" trileSetName=\"S\"><Triles>" + validTrile + "</Triles><ArtObjects>"
                            + validArtObject + "</ArtObjects>";

    checkFailures({
        {"no BackgroundPlanes", head + "<NonplayerCharacters>" + validNpc + "</NonplayerCharacters></Level>",
         ErrorKind::MissingElement, "Level/BackgroundPlanes"},
        {"no NonplayerCharacters", head + "<BackgroundPlanes>" + validPlane + "</BackgroundPlanes></Level>",
         ErrorKind::MissingElement, "Level/NonplayerCharacters"},
    });
}

TEST_CASE("readLevel fails the whole level for a broken background plane entry", "[level]")
{
    const auto broken = [](const QByteArray& entry) {
        return level(validTrile, validArtObject, validPlane + "<Entry>" + entry + "</Entry>", validNpc);
    };
    const QByteArray position = "<Position><Vector3 x=\"1\" y=\"2\" z=\"3\"/></Position>";
    const QByteArray rotation = "<Rotation><Quaternion x=\"0\" y=\"0\" z=\"0\" w=\"1\"/></Rotation>";
    const QByteArray scale = "<Scale><Vector3 x=\"1\" y=\"1\" z=\"1\"/></Scale>";
    const auto plane = [](const QByteArray& attributes, const QByteArray& content) {
        return "<BackgroundPlane " + attributes + ">" + content + "</BackgroundPlane>";
    };
    const QByteArray valid = "textureName=\"t\" animated=\"false\" doubleSided=\"true\" opacity=\"1\"";
    const QByteArray all = position + rotation + scale;
    const std::string entry = "Level/BackgroundPlanes/Entry[2]/BackgroundPlane";

    checkFailures({
        {"no BackgroundPlane", broken(""), ErrorKind::MissingElement, entry},
        {"textureName missing", broken(plane("animated=\"false\" doubleSided=\"true\" opacity=\"1\"", all)),
         ErrorKind::MissingAttribute, entry + "@textureName"},
        {"animated missing", broken(plane("textureName=\"t\" doubleSided=\"true\" opacity=\"1\"", all)),
         ErrorKind::MissingAttribute, entry + "@animated"},
        {"animated invalid", broken(plane("textureName=\"t\" animated=\"yes\" doubleSided=\"true\" opacity=\"1\"", all)),
         ErrorKind::BadValue, entry + "@animated"},
        {"animated empty", broken(plane("textureName=\"t\" animated=\"\" doubleSided=\"true\" opacity=\"1\"", all)),
         ErrorKind::BadValue, entry + "@animated"},
        {"opacity missing", broken(plane("textureName=\"t\" animated=\"false\" doubleSided=\"true\"", all)),
         ErrorKind::MissingAttribute, entry + "@opacity"},
        {"opacity not a number",
         broken(plane("textureName=\"t\" animated=\"false\" doubleSided=\"true\" opacity=\"half\"", all)),
         ErrorKind::BadValue, entry + "@opacity"},
        {"doubleSided missing", broken(plane("textureName=\"t\" animated=\"false\" opacity=\"1\"", all)),
         ErrorKind::MissingAttribute, entry + "@doubleSided"},
        {"doubleSided invalid",
         broken(plane("textureName=\"t\" animated=\"false\" doubleSided=\"1\" opacity=\"1\"", all)),
         ErrorKind::BadValue, entry + "@doubleSided"},
        {"no Position", broken(plane(valid, rotation + scale)), ErrorKind::MissingElement, entry + "/Position"},
        {"no Position/Vector3", broken(plane(valid, "<Position/>" + rotation + scale)), ErrorKind::MissingElement,
         entry + "/Position/Vector3"},
        {"position y not a number",
         broken(plane(valid, "<Position><Vector3 x=\"1\" y=\"y\" z=\"3\"/></Position>" + rotation + scale)),
         ErrorKind::BadValue, entry + "/Position/Vector3@y"},
        {"no Rotation", broken(plane(valid, position + scale)), ErrorKind::MissingElement, entry + "/Rotation"},
        {"no Rotation/Quaternion", broken(plane(valid, position + "<Rotation/>" + scale)), ErrorKind::MissingElement,
         entry + "/Rotation/Quaternion"},
        {"quaternion x missing",
         broken(plane(valid, position + "<Rotation><Quaternion y=\"0\" z=\"0\" w=\"1\"/></Rotation>" + scale)),
         ErrorKind::MissingAttribute, entry + "/Rotation/Quaternion@x"},
        {"no Scale", broken(plane(valid, position + rotation)), ErrorKind::MissingElement, entry + "/Scale"},
        {"no Scale/Vector3", broken(plane(valid, position + rotation + "<Scale/>")), ErrorKind::MissingElement,
         entry + "/Scale/Vector3"},
        {"scale z missing", broken(plane(valid, position + rotation + "<Scale><Vector3 x=\"1\" y=\"1\"/></Scale>")),
         ErrorKind::MissingAttribute, entry + "/Scale/Vector3@z"},
    });
}

TEST_CASE("readLevel fails the whole level for a broken NPC entry", "[level]")
{
    const auto broken = [](const QByteArray& entry) {
        return level(validTrile, validArtObject, validPlane, validNpc + "<Entry>" + entry + "</Entry>");
    };
    const QByteArray position = "<Position><Vector3 x=\"1\" y=\"2\" z=\"3\"/></Position>";
    const QByteArray actions = "<Actions><Action key=\"Idle\"><NpcActionContent animationName=\"idle\"/></Action></Actions>";
    const auto npc = [](const QByteArray& content) { return "<NpcInstance name=\"n\">" + content + "</NpcInstance>"; };
    const std::string entry = "Level/NonplayerCharacters/Entry[2]/NpcInstance";

    checkFailures({
        {"no NpcInstance", broken(""), ErrorKind::MissingElement, entry},
        {"name missing", broken("<NpcInstance>" + position + actions + "</NpcInstance>"), ErrorKind::MissingAttribute,
         entry + "@name"},
        {"no Actions", broken(npc(position)), ErrorKind::MissingElement, entry + "/Actions"},
        {"no actions", broken(npc(position + "<Actions/>")), ErrorKind::MissingElement, entry + "/Actions/Action"},
        {"only non-Action elements", broken(npc(position + "<Actions><Other key=\"k\"/></Actions>")),
         ErrorKind::MissingElement, entry + "/Actions/Action"},
        {"action key missing",
         broken(npc(position + "<Actions><Action><NpcActionContent animationName=\"idle\"/></Action></Actions>")),
         ErrorKind::MissingAttribute, entry + "/Actions/Action@key"},
        {"no NpcActionContent", broken(npc(position + "<Actions><Action key=\"Idle\"/></Actions>")),
         ErrorKind::MissingElement, entry + "/Actions/Action/NpcActionContent"},
        {"animationName missing",
         broken(npc(position + "<Actions><Action key=\"Idle\"><NpcActionContent/></Action></Actions>")),
         ErrorKind::MissingAttribute, entry + "/Actions/Action/NpcActionContent@animationName"},
        {"broken second action",
         broken(npc(position + "<Actions><Action key=\"Idle\"><NpcActionContent animationName=\"idle\"/></Action>"
                               "<Action key=\"Walk\"/></Actions>")),
         ErrorKind::MissingElement, entry + "/Actions/Action[2]/NpcActionContent"},
        {"no Position", broken(npc(actions)), ErrorKind::MissingElement, entry + "/Position"},
        {"no Position/Vector3", broken(npc("<Position/>" + actions)), ErrorKind::MissingElement,
         entry + "/Position/Vector3"},
        {"position x not a number", broken(npc("<Position><Vector3 x=\"a\" y=\"2\" z=\"3\"/></Position>" + actions)),
         ErrorKind::BadValue, entry + "/Position/Vector3@x"},
    });
}

TEST_CASE("readLevel reads plane, NPC and action entries from the first one on, whatever the following names", "[level]")
{
    const QByteArray otherPlane = "<Other><BackgroundPlane textureName=\"other\" animated=\"true\" doubleSided=\"false\" opacity=\"0\">"
                                  "<Position><Vector3 x=\"1\" y=\"2\" z=\"3\"/></Position>"
                                  "<Rotation><Quaternion x=\"0\" y=\"0\" z=\"0\" w=\"1\"/></Rotation>"
                                  "<Scale><Vector3 x=\"1\" y=\"1\" z=\"1\"/></Scale></BackgroundPlane></Other>";
    const QByteArray npc = "<Entry><NpcInstance name=\"npc\">"
                           "<Position><Vector3 x=\"1\" y=\"2\" z=\"3\"/></Position>"
                           "<Actions><Skipped key=\"no\"/><Action key=\"Idle\"><NpcActionContent animationName=\"idle\"/></Action>"
                           "<Other key=\"Walk\"><NpcActionContent animationName=\"walk\"/></Other></Actions>"
                           "</NpcInstance></Entry>";
    const QByteArray otherNpc = "<Other><NpcInstance name=\"other\">"
                                "<Position><Vector3 x=\"1\" y=\"2\" z=\"3\"/></Position>"
                                "<Actions><Action key=\"Idle\"><NpcActionContent animationName=\"idle\"/></Action></Actions>"
                                "</NpcInstance></Other>";

    const auto description = readInline(
        level(validTrile, validArtObject, "<Skipped/>" + validPlane + otherPlane, "<Skipped/>" + npc + otherNpc));
    REQUIRE(description.has_value());

    REQUIRE(description->m_BackgroundPlanes.size() == 2);
    CHECK(description->m_BackgroundPlanes[1].m_TextureName == QStringLiteral("other"));

    REQUIRE(description->m_Npcs.size() == 2);
    CHECK(description->m_Npcs[1].m_Name == QStringLiteral("other"));

    // Actions: elements before the first Action are skipped, every element after it is an action.
    REQUIRE(description->m_Npcs[0].m_Actions.size() == 2);
    CHECK(description->m_Npcs[0].m_Actions[0].first == QStringLiteral("Idle"));
    CHECK(description->m_Npcs[0].m_Actions[1].first == QStringLiteral("Walk"));
    CHECK(description->m_Npcs[0].m_Actions[1].second == QStringLiteral("walk"));

    checkFailures({
        {"trailing non-entry in BackgroundPlanes", level(validTrile, validArtObject, validPlane + "<Comment/>", validNpc),
         ErrorKind::MissingElement, "Level/BackgroundPlanes/Comment/BackgroundPlane"},
        {"trailing non-entry in NonplayerCharacters", level(validTrile, validArtObject, validPlane, validNpc + "<Comment/>"),
         ErrorKind::MissingElement, "Level/NonplayerCharacters/Comment/NpcInstance"},
    });
}

TEST_CASE("readLevel checks ArtObjects, BackgroundPlanes and NonplayerCharacters in this order", "[level]")
{
    const auto artObjectsFirst = readInline(level(validTrile, "<Entry/>", "<Entry/>", "<Entry/>"));
    REQUIRE_FALSE(artObjectsFirst.has_value());
    CHECK(artObjectsFirst.error().source == "Level/ArtObjects/Entry/ArtObjectInstance");

    const auto planesFirst = readInline(level(validTrile, validArtObject, "<Entry/>", "<Entry/>"));
    REQUIRE_FALSE(planesFirst.has_value());
    CHECK(planesFirst.error().source == "Level/BackgroundPlanes/Entry/BackgroundPlane");
}
