#include "parser/TrileSetParser.h"

#include "writer/GeometryWriter.h"

#include <QtCore/QFile>
#include <QtCore/QFileInfo>
#include <QtCore/QDir>

TrileSetParser::TrileSetParser() : m_Document{}
{
}

TrileSetParser::~TrileSetParser()
{
}

void TrileSetParser::parse(const QString& path) noexcept
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
    const auto trile_set_elem = m_Document.firstChildElement("TrileSet");

    if(trile_set_elem.isNull())
        return;

    if(!trile_set_elem.hasAttribute("name"))
        return;

    const auto set_name = trile_set_elem.attribute("name");

    QDir set_dir(out_path);

    if(!set_dir.exists(set_name))
        set_dir.mkdir(set_name);

    const auto triles_elem = trile_set_elem.firstChildElement("Triles");

    if(triles_elem.isNull())
        return;

    // count vertices
    auto trile_entry_elem = triles_elem.firstChildElement("TrileEntry");

    while(!trile_entry_elem.isNull())
    {
        if(!trile_entry_elem.hasAttribute("key"))
            return;

        const auto key = trile_entry_elem.attribute("key");
        const auto trile_elem = trile_entry_elem.firstChildElement("Trile");

        qDebug() << "\t: " << key;

        if(trile_elem.isNull())
            return;

        const auto geom_elem = trile_elem.firstChildElement("Geometry");

        if(geom_elem.isNull())
            return;

        auto geometry = GeometryParser().parseGeometry(geom_elem.firstChildElement("ShaderInstancedIndexedPrimitives"));

        if(!geometry)
            return;

        geometry->m_Name = set_name + "/" + key;
        geometry->m_TextureName = set_name;

        GeometryWriter(out_path).writeObj(*geometry);

        QFile texture(org_path + "/" + name + ".png");
        texture.copy(out_path + "/" + set_name + "/" + name + ".png");

        trile_entry_elem = trile_entry_elem.nextSiblingElement();
    }
}