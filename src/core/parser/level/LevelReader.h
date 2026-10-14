#pragma once

#include "error/Error.h"
#include "model/LevelDescription.h"

#include <QtCore/QString>
#include <QtXml/QDomDocument>

#include <expected>

namespace fez
{

// Reads a level XML file into a LevelDescription; no referenced file is loaded.
// Errors: FileNotFound, Io, XmlSyntax (loadXml); the errors of readLevel.
std::expected<LevelDescription, Error> parseLevel(const QString& path);

// Reads a loaded level document. Parts, in this order (the first error is returned):
//  - header: the Level element (QDomDocument::firstChildElement; MissingElement with source "Level"), its
//    trileSetName and name attributes (MissingAttribute)
//  - Triles: one TrileEmplacement per entry (TrileEmplacement x/y/z, TrileInstance trileId/orientation read as
//    integers, orientation 0 to 3 (BadValue otherwise), Position/Vector3), followed directly by one TrileEmplacement
//    per child element of TrileInstance/OverlappedTriles (each read like a TrileInstance, at the entry's emplacement)
//  - ArtObjects: one ArtObject per entry (ArtObjectInstance name, Position/Vector3, Rotation/Quaternion read as
//    w, x, y, z, Scale/Vector3)
//  - BackgroundPlanes: one BackgroundPlaneDescription per entry (BackgroundPlane textureName with every '\' replaced
//    by '/', animated, opacity, doubleSided, Position/Vector3, Rotation/Quaternion as w, x, y, z, Scale/Vector3)
//  - NonplayerCharacters: one NpcDescription per entry (NpcInstance name, Actions, Position/Vector3). Actions start
//    at the first Action child and every following sibling element is an action (key, NpcActionContent
//    animationName); no Action child fails the level (MissingElement ".../Actions/Action").
// All four sections are mandatory (MissingElement). Entries start at the first Entry child; every following sibling
// element is read as an entry, whatever its name (as before). Any missing element (MissingElement), missing attribute
// (MissingAttribute), unparsable number or boolean other than true/false, case-insensitive (BadValue) fails the whole
// level. No texture is loaded here.
std::expected<LevelDescription, Error> readLevel(const QDomDocument& document);

} // namespace fez
