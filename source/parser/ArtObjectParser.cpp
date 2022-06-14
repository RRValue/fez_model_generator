#include "parser/ArtObjectParser.h"

#include "parser/GeometryParser.h"
#include "writer/GeometryWriter.h"

#include <QtCore/QDir>
#include <QtCore/QFile>
#include <QtCore/QFileInfo>

#include <qdebug.h>

ArtObjectParser::ArtObjectParser() : m_Document{}
{
}

ArtObjectParser::~ArtObjectParser()
{
}

void ArtObjectParser::parse(const QString& path) noexcept
{
    qDebug() << "parsing: " << path;

    QFileInfo info(path);

    if(!info.exists())
        return;

    static const auto out_folder_name = "exported";

    const auto name = info.baseName();
    const auto org_path = info.absolutePath();
    auto out_path = info.absolutePath();
    m_Document.clear();

    QDir dir(out_path);

    if(!dir.exists(out_folder_name))
        dir.mkdir(out_folder_name);

    out_path = out_path + "/" + out_folder_name;

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

    auto geometry = GeometryParser().parseGeometry(art_obj_elem.firstChildElement("ShaderInstancedIndexedPrimitives"));

    if(!geometry)
        return;

    geometry->m_Name = name;
    geometry->m_TextureName = name;

    GeometryWriter(out_path).writeObj(*geometry);

    QFile texture(org_path + "/" + name + ".png");
    texture.copy(out_path + "/" + name + ".png");
}
