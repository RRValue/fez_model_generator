#pragma once

#include "error/Error.h"
#include "model/Geometry.h"

#include <QtCore/QString>

#include <expected>

namespace fez
{

// Reads an art object XML file (ArtObject document element with a ShaderInstancedIndexedPrimitives child).
//  - m_Name: the file's base name (up to the first dot, QFileInfo::baseName)
//  - texture: "<name>.png" in the directory of the XML file (not checked for existence)
//  - vertices and indices: parseGeometry on ArtObject/ShaderInstancedIndexedPrimitives
// Errors: FileNotFound, Io, XmlSyntax (loadXml); MissingElement (no ArtObject document element, source "ArtObject";
// no geometry child); errors of parseGeometry.
std::expected<Geometry, Error> parseArtObject(const QString& path);

} // namespace fez
