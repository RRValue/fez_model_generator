#include "parser/TrileSetParser.h"

#include "parser/GeometryParser.h"

#include <QtCore/QFile>
#include <QtCore/QFileInfo>
#include <QtCore/QDir>

TrileSetParser::TrileSetParser() : m_Document{}
{
}

TrileSetParser::~TrileSetParser()
{
}

TrileSetParser::GeometryResults TrileSetParser::parse(const QString& path) noexcept
{
    qDebug() << "parsing: " << path;

    QFileInfo info(path);

    if(!info.exists())
        return {};

    static const auto out_folder_name = "exported";

    m_Name = info.baseName();
    m_OrgPath = info.absolutePath();
    m_OutPath = info.absolutePath();
    m_Document.clear();

    QDir dir(m_OutPath);

    if(!dir.exists(out_folder_name))
        dir.mkdir(out_folder_name);

    m_OutPath = m_OutPath + "/" + out_folder_name;

    // load
    auto xml_file = QFile(path);

    if(!xml_file.exists())
        return {};

    if(!xml_file.open(QIODevice::OpenModeFlag::ReadOnly))
        return {};

    QString error_msg;
    int error_line;
    int error_column;

    if(!m_Document.setContent(&xml_file, &error_msg, &error_line, &error_column))
    {
        qDebug() << "Error: \"" + error_msg + "\" at line: " + QString::number(error_line) + " Columns: " + QString::number(error_column);

        return {};
    }

    // parse
    const auto trile_set_elem = m_Document.firstChildElement("TrileSet");

    if(trile_set_elem.isNull())
        return {};

    if(!trile_set_elem.hasAttribute("name"))
        return {};

    m_SetName = trile_set_elem.attribute("name");

    QDir set_dir(m_OutPath);

    if(!set_dir.exists(m_SetName))
        set_dir.mkdir(m_SetName);

    const auto triles_elem = trile_set_elem.firstChildElement("Triles");

    if(triles_elem.isNull())
        return {};

    auto trile_entry_elem = triles_elem.firstChildElement("TrileEntry");

    GeometryResults results;

    while(!trile_entry_elem.isNull())
    {
        auto result = parserTrile(trile_entry_elem);

        if(result)
            results.push_back(std::move(*result));

        trile_entry_elem = trile_entry_elem.nextSiblingElement();
    }

    return results;
}

TrileSetParser::GeometryResult TrileSetParser::parserTrile(const QDomElement& elem)
{
    if(!elem.hasAttribute("key"))
        return {};

    const auto key = elem.attribute("key");
    const auto trile_elem = elem.firstChildElement("Trile");

    qDebug() << "\t: " << key;

    if(trile_elem.isNull())
        return {};

    const auto geom_elem = trile_elem.firstChildElement("Geometry");

    if(geom_elem.isNull())
        return {};

    auto geometry = GeometryParser().parseGeometry(geom_elem.firstChildElement("ShaderInstancedIndexedPrimitives"));

    if(!geometry)
        return {};

    geometry->m_Name = key;
    geometry->m_TextureName = m_SetName + ".png";
    geometry->m_TextureOrgFile = m_OrgPath + "/" + m_Name + ".png";
    geometry->m_OutPath = m_OutPath + "/" + m_SetName;

    return geometry;
}