#pragma once

#include "error/Error.h"
#include "model/Geometry.h"

#include <QtCore/QString>

#include <expected>
#include <map>

namespace fez
{

struct TrileSet
{
    using Triles = std::map<int, Geometry>;

    QString m_Name = {};
    Triles m_Triles = {};
};

// Reads a trile set XML file (TrileSet document element with a name attribute and a Triles child of TrileEntry
// elements). Nothing is written anywhere.
//  - m_Name: the TrileSet name attribute
//  - m_Triles: one geometry per TrileEntry, keyed by its key attribute (QString::toInt); for duplicate keys the first
//    entry wins. Iteration starts at the first TrileEntry and then visits every following sibling element, whatever
//    its name (as before).
//  - geometry name "<key>_<trile name>" (key text as written), texture name "<set name>.png", texture file
//    "<file base name>.png" in the directory of the XML file (not checked for existence)
//  - vertices and indices: parseGeometry on Trile/Geometry/ShaderInstancedIndexedPrimitives
// An empty Triles element gives a set without triles (callers treat that as a failure).
// Errors: FileNotFound, Io, XmlSyntax (loadXml); MissingElement (no TrileSet document element, source "TrileSet";
// missing Triles, Trile, Geometry or ShaderInstancedIndexedPrimitives); MissingAttribute (set name, key, trile name);
// BadValue (key not an int); errors of parseGeometry. Any failing entry fails the whole set.
std::expected<TrileSet, Error> parseTrileSet(const QString& path);

} // namespace fez
