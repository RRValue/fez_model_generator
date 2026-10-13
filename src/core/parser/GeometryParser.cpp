#include "parser/GeometryParser.h"

#include "geometry/FaceDecoding.h"

#include <QtCore/QDir>
#include <QtCore/QFile>
#include <QtCore/QFileInfo>

#include <assimp/Exporter.hpp>
#include <assimp/scene.h>

#include <qdebug.h>

namespace fez
{

GeometryParser::GeometryParser() : m_Document{}
{
}

GeometryParser::~GeometryParser()
{
}

GeometryParser::GeometryResult GeometryParser::parseGeometry(const QDomElement& elem)
{
    if(elem.isNull() || elem.nodeName() != "ShaderInstancedIndexedPrimitives")
        return {};

    auto vertices = parseVertices(elem.firstChildElement("Vertices"));

    if(!vertices)
        return {};

    auto indices = parseIndices(elem.firstChildElement("Indices"));

    if(!indices)
        return {};

    Geometry result;
    result.m_Vertices = std::move(*vertices);
    result.m_Indices = std::move(*indices);

    return result;
}

GeometryParser::VerticesResult GeometryParser::parseVertices(const QDomElement& elem)
{
    if(elem.isNull())
        return {};

    VerticesResult::value_type result;

    // count vertices
    auto vertex_elem = elem.firstChildElement("VertexPositionNormalTextureInstance");
    auto vertex_count = size_t(0);

    while(!vertex_elem.isNull())
    {
        vertex_count++;

        vertex_elem = vertex_elem.nextSiblingElement();
    }

    result.resize(vertex_count);

    // set vertices
    vertex_elem = elem.firstChildElement("VertexPositionNormalTextureInstance");
    vertex_count = size_t(0);

    while(!vertex_elem.isNull())
    {
        const auto position_elem = vertex_elem.firstChildElement("Position");
        const auto normal_elem = vertex_elem.firstChildElement("Normal");
        const auto texture_coord_elem = vertex_elem.firstChildElement("TextureCoord");

        if(position_elem.isNull() || normal_elem.isNull() || texture_coord_elem.isNull())
            return {};

        auto& vertex = result[vertex_count];

        // position
        const auto vec3_elem = position_elem.firstChildElement("Vector3");

        if(vec3_elem.isNull())
            return {};

        if(!vec3_elem.hasAttribute("x") || !vec3_elem.hasAttribute("y") || !vec3_elem.hasAttribute("z"))
            return {};

        auto x_ok = false;
        auto y_ok = false;
        auto z_ok = false;

        vertex.m_Position.x() = vec3_elem.attribute("x").toFloat(&x_ok);
        vertex.m_Position.y() = vec3_elem.attribute("y").toFloat(&y_ok);
        vertex.m_Position.z() = vec3_elem.attribute("z").toFloat(&z_ok);

        if(!x_ok || !y_ok || !z_ok)
            return {};

        // normal
        auto side_index_ok = false;
        const auto side_index = normal_elem.text().toInt(&side_index_ok);

        if(!side_index_ok)
            return {};

        const auto normal = decodeFaceNormal(side_index);

        if(!normal)
            return {};

        vertex.m_Normal = *normal;

        // texture coordinate
        const auto vec2_elem = texture_coord_elem.firstChildElement("Vector2");

        if(vec2_elem.isNull())
            return {};

        if(!vec2_elem.hasAttribute("x") || !vec2_elem.hasAttribute("y"))
            return {};

        x_ok = false;
        y_ok = false;

        vertex.m_TextureCoordinate.x() = vec2_elem.attribute("x").toFloat(&x_ok);
        vertex.m_TextureCoordinate.y() = 1.0f - vec2_elem.attribute("y").toFloat(&y_ok);

        // Side 0 adds nothing (as before), so a U of -0 stays -0.
        if(const auto uv_offset = faceUvOffset(side_index); uv_offset != 0.0f)
            vertex.m_TextureCoordinate.x() += uv_offset;

        if(!x_ok || !y_ok)
            return {};

        vertex_count++;
        vertex_elem = vertex_elem.nextSiblingElement();
    }

    return result;
}

GeometryParser::IndicesResult GeometryParser::parseIndices(const QDomElement& elem)
{
    if(elem.isNull())
        return {};

    IndicesResult::value_type result;

    // count indices
    auto index_elem = elem.firstChildElement("Index");
    auto index_count = size_t(0);

    while(!index_elem.isNull())
    {
        index_count++;

        index_elem = index_elem.nextSiblingElement();
    }

    result.resize(index_count);

    if(index_count % 3 != 0)
        return {};

    // set indices
    index_elem = elem.firstChildElement("Index");
    index_count = size_t(0);

    while(!index_elem.isNull())
    {
        auto index_ok = false;
        result[index_count] = index_elem.text().toInt(&index_ok);

        if(!index_ok)
            return {};

        index_count++;
        index_elem = index_elem.nextSiblingElement();
    }

    return result;
}

} // namespace fez
