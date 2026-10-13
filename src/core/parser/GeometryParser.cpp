#include "parser/GeometryParser.h"

#include "geometry/FaceDecoding.h"
#include "format/QStringFormatter.h"
#include "xml/Xml.h"

#include <QtCore/QString>

#include <format>
#include <ranges>
#include <utility>

namespace fez
{

namespace
{

// The first child element named name and every following sibling element, whatever its name (the original parser
// walked nextSiblingElement() without a name). Empty when there is no such child.
auto elementsFrom(const QDomElement& parent, const QString& name)
{
    return std::ranges::subrange(ChildElementRange::Iterator(parent.firstChildElement(name), QString()),
                                 ChildElementRange::Iterator());
}

// Text of element converted with QString::toInt.
std::expected<int, Error> readIntText(const QDomElement& element)
{
    const auto text = element.text();
    auto ok = false;
    const auto value = text.toInt(&ok);

    if(!ok)
        return std::unexpected(
            Error{ErrorKind::BadValue, std::format("text is not a valid int: '{}'", text), elementPath(element)});

    return value;
}

std::expected<Vertex, Error> parseVertex(const QDomElement& elem)
{
    const auto position_elem = childElement(elem, QStringLiteral("Position"));
    if(!position_elem)
        return std::unexpected(position_elem.error());

    const auto normal_elem = childElement(elem, QStringLiteral("Normal"));
    if(!normal_elem)
        return std::unexpected(normal_elem.error());

    const auto texture_coord_elem = childElement(elem, QStringLiteral("TextureCoord"));
    if(!texture_coord_elem)
        return std::unexpected(texture_coord_elem.error());

    Vertex vertex;

    // position
    const auto vec3_elem = childElement(*position_elem, QStringLiteral("Vector3"));
    if(!vec3_elem)
        return std::unexpected(vec3_elem.error());

    const auto position = readVector3(*vec3_elem);
    if(!position)
        return std::unexpected(position.error());

    vertex.m_Position = *position;

    // normal
    const auto side_index = readIntText(*normal_elem);
    if(!side_index)
        return std::unexpected(side_index.error());

    const auto normal = decodeFaceNormal(*side_index);
    if(!normal)
        return std::unexpected(Error{ErrorKind::BadValue,
                                     std::format("normal side is not in 0-5: {}", *side_index),
                                     elementPath(*normal_elem)});

    vertex.m_Normal = *normal;

    // texture coordinate
    const auto vec2_elem = childElement(*texture_coord_elem, QStringLiteral("Vector2"));
    if(!vec2_elem)
        return std::unexpected(vec2_elem.error());

    const auto texture_coordinate = readVector2(*vec2_elem);
    if(!texture_coordinate)
        return std::unexpected(texture_coordinate.error());

    vertex.m_TextureCoordinate.x() = texture_coordinate->x();
    vertex.m_TextureCoordinate.y() = 1.0f - texture_coordinate->y();

    // Side 0 adds nothing (as before), so a U of -0 stays -0.
    if(const auto uv_offset = faceUvOffset(*side_index); uv_offset != 0.0f)
        vertex.m_TextureCoordinate.x() += uv_offset;

    return vertex;
}

std::expected<Geometry::Vertices, Error> parseVertices(const QDomElement& elem)
{
    Geometry::Vertices result;

    for(const auto& vertex_elem : elementsFrom(elem, QStringLiteral("VertexPositionNormalTextureInstance")))
    {
        auto vertex = parseVertex(vertex_elem);
        if(!vertex)
            return std::unexpected(std::move(vertex.error()));

        result.push_back(*vertex);
    }

    return result;
}

std::expected<Geometry::Indices, Error> parseIndices(const QDomElement& elem)
{
    const auto index_elems = elementsFrom(elem, QStringLiteral("Index"));
    const auto index_count = static_cast<size_t>(std::ranges::distance(index_elems));

    if(index_count % 3 != 0)
        return std::unexpected(Error{ErrorKind::BadValue,
                                     std::format("index count is not divisible by 3: {}", index_count),
                                     elementPath(elem)});

    Geometry::Indices result;
    result.reserve(index_count);

    for(const auto& index_elem : index_elems)
    {
        const auto index = readIntText(index_elem);
        if(!index)
            return std::unexpected(index.error());

        // Stored in size_t as before: a negative index wraps.
        result.push_back(static_cast<size_t>(*index));
    }

    return result;
}

} // namespace

std::expected<Geometry, Error> parseGeometry(const QDomElement& element)
{
    const auto expected_name = QStringLiteral("ShaderInstancedIndexedPrimitives");

    if(element.isNull())
        return std::unexpected(
            Error{ErrorKind::MissingElement, std::format("missing element '{}'", expected_name), expected_name.toStdString()});

    if(element.nodeName() != expected_name)
        return std::unexpected(Error{ErrorKind::BadValue,
                                     std::format("expected element '{}', found '{}'", expected_name, element.nodeName()),
                                     elementPath(element)});

    const auto vertices_elem = childElement(element, QStringLiteral("Vertices"));
    if(!vertices_elem)
        return std::unexpected(vertices_elem.error());

    auto vertices = parseVertices(*vertices_elem);
    if(!vertices)
        return std::unexpected(std::move(vertices.error()));

    const auto indices_elem = childElement(element, QStringLiteral("Indices"));
    if(!indices_elem)
        return std::unexpected(indices_elem.error());

    auto indices = parseIndices(*indices_elem);
    if(!indices)
        return std::unexpected(std::move(indices.error()));

    Geometry result;
    result.m_Vertices = std::move(*vertices);
    result.m_Indices = std::move(*indices);

    return result;
}

} // namespace fez
