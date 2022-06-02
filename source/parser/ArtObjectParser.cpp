#include "ArtObjectParser.h"

#include <QtCore/QFile>

#include <qdebug.h>

ArtObjectParser::ArtObjectParser() : m_Document{}
{
}

ArtObjectParser::~ArtObjectParser()
{
}

void ArtObjectParser::parse(const QString& path) noexcept
{
    m_Document.clear();

    // load
    auto xml_file = QFile(path);

    if(!xml_file.exists())
        return;

    if(!xml_file.open(QIODevice::OpenModeFlag::ReadOnly))
        return;

    QString error_msg;
    int error_line;
    int error_column;

    if(!m_Document.setContent(&xml_file, &error_msg, &error_line, &error_column))
    {
        qDebug() << "Error: \"" + error_msg + "\" at line: " + QString::number(error_line) + " Columns: " + QString::number(error_column);

        return;
    }

    // parse
    const auto art_obj_elem = m_Document.firstChildElement("ArtObject");

    if(art_obj_elem.isNull())
        return;

    const auto geometry = parseGeometry(art_obj_elem.firstChildElement("ShaderInstancedIndexedPrimitives"));
}

ArtObjectParser::GeometryResult ArtObjectParser::parseGeometry(const QDomElement& elem)
{
    if(elem.isNull())
        return {};

    auto vertices = parseVertices(elem.firstChildElement("Vertices"));

    if(!vertices)
        return {};

    auto triangles = parseTriangles(elem.firstChildElement("Indices"));

    if(!triangles)
        return {};

    Geometry result;
    result.m_Vertices = std::move(*vertices);
    result.m_Triangles = std::move(*triangles);

    return result;
}

ArtObjectParser::VerticesResult ArtObjectParser::parseVertices(const QDomElement& elem)
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
        auto normal_index_ok = false;
        const auto normal_index = normal_elem.text().toInt(&normal_index_ok);

        if(!normal_index_ok)
            return {};

        switch(normal_index)
        {
            case 0: vertex.m_Normal = {0.0f, 0.0f, 0.0f}; break;
            case 1: vertex.m_Normal = {1.0f, 1.0f, 1.0f}; break;
            case 2: vertex.m_Normal = {2.0f, 2.0f, 2.0f}; break;
            case 3: vertex.m_Normal = {3.0f, 3.0f, 3.0f}; break;
            case 4: vertex.m_Normal = {4.0f, 4.0f, 4.0f}; break;
            case 5: vertex.m_Normal = {5.0f, 5.0f, 5.0f}; break;
            default: return {};
        }

        // texture coordinate
        const auto vec2_elem = texture_coord_elem.firstChildElement("Vector2");

        if(vec2_elem.isNull())
            return {};

        if(!vec2_elem.hasAttribute("x") || !vec2_elem.hasAttribute("y"))
            return {};

        x_ok = false;
        y_ok = false;

        vertex.m_TextureCoordinate.x() = vec2_elem.attribute("x").toFloat(&x_ok);
        vertex.m_TextureCoordinate.y() = vec2_elem.attribute("y").toFloat(&y_ok);

        if(!x_ok || !y_ok)
            return {};

        vertex_count++;
        vertex_elem = vertex_elem.nextSiblingElement();
    }

    return result;
}

ArtObjectParser::TrianglesResult ArtObjectParser::parseTriangles(const QDomElement& elem)
{
    if(elem.isNull())
        return {};

    TrianglesResult::value_type result;

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