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

    const auto geom_elem = art_obj_elem.firstChildElement("ShaderInstancedIndexedPrimitives");

    if(geom_elem.isNull())
        return;

    const auto vertices_elem = geom_elem.firstChildElement("Vertices");

    if(vertices_elem.isNull())
        return;

    auto vertex_elem = vertices_elem.firstChildElement("VertexPositionNormalTextureInstance");

    while(!vertex_elem.isNull())
    {
        const auto position_elem = vertex_elem.firstChildElement("Position");

        if(position_elem.isNull())
            return;

        const auto vec_elem = position_elem.firstChildElement("Vector3");

        if(vec_elem.isNull())
            return;

        vertex_elem = vertex_elem.nextSiblingElement();
    }
}