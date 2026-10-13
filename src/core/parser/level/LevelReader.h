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
//  - Triles: one TrileEmplacement per entry (TrileEmplacement x/y/z, TrileInstance trileId/orientation and
//    Position/Vector3, all read with QString::toFloat; trileId and orientation are then converted to int / unsigned)
//  - ArtObjects: one ArtObject per entry (ArtObjectInstance name, Position/Vector3, Rotation/Quaternion read as
//    w, x, y, z, Scale/Vector3)
// Both sections are mandatory (MissingElement). Entries start at the first Entry child; every following sibling
// element is read as an entry, whatever its name (as before). Any missing element (MissingElement), missing attribute
// (MissingAttribute) or unparsable number (BadValue) fails the whole level.
// BackgroundPlanes and NonplayerCharacters are not read yet (Task 19); m_BackgroundPlanes and m_Npcs stay empty.
std::expected<LevelDescription, Error> readLevel(const QDomDocument& document);

} // namespace fez
