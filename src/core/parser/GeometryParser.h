#pragma once

#include "error/Error.h"
#include "model/Geometry.h"

#include <QtXml/QDomElement>

#include <expected>

namespace fez
{

// Reads the vertices and indices of a ShaderInstancedIndexedPrimitives element. Only m_Vertices and m_Indices are set;
// the caller names the geometry and sets its texture.
//  - vertices: Position/Vector3, Normal (side index 0-5, decoded with decodeFaceNormal), TextureCoord/Vector2 with V
//    flipped (1 - v) and U shifted by faceUvOffset(side)
//  - indices: Index texts read as int and stored as written (winding unchanged); their count must be divisible by 3
//    and each must be a non-negative index below the vertex count
//  - as in the original parser, iteration starts at the first Vertices/VertexPositionNormalTextureInstance (or
//    Indices/Index) child and then visits every following sibling element, whatever its name
// Errors: MissingElement (null element, missing child element), BadValue (wrong element name, bad number, normal
// side outside 0-5, index count not divisible by 3, negative index or index >= vertex count, source the Index
// element), MissingAttribute (Vector2/Vector3 attribute).
std::expected<Geometry, Error> parseGeometry(const QDomElement& element);

} // namespace fez
