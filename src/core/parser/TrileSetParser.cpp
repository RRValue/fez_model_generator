#include "parser/TrileSetParser.h"

#include "parser/GeometryParser.h"

#include <QtCore/QDir>
#include <QtCore/QFile>
#include <QtCore/QFileInfo>

TrileSetParser::TrileSetParser() : m_Document{}
{
}

TrileSetParser::~TrileSetParser()
{
}

const QString& TrileSetParser::getSetName() const noexcept
{
    return m_SetName;
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

    // read trile entries
    auto trile_entry_elem = triles_elem.firstChildElement("TrileEntry");

    GeometryResults results;

    while(!trile_entry_elem.isNull())
    {
        auto result = parserTrile(trile_entry_elem);

        if(!result)
            return {};

        results.insert(std::move(*result));
        trile_entry_elem = trile_entry_elem.nextSiblingElement();
    }

    return results;
}

TrileSetParser::TrileResult TrileSetParser::parserTrile(const QDomElement& elem)
{
    if(!elem.hasAttribute("key"))
        return {};

    const auto key_str = elem.attribute("key");
    const auto trile_elem = elem.firstChildElement("Trile");

    qDebug() << "\t: " << key_str;

    if(trile_elem.isNull())
        return {};

    auto key_ok = false;
    const auto key = key_str.toInt(&key_ok);

    if(!key_ok)
        return {};

    if(!trile_elem.hasAttribute("name"))
        return {};

    const auto name = trile_elem.attribute("name");

    const auto geom_elem = trile_elem.firstChildElement("Geometry");

    if(geom_elem.isNull())
        return {};

    auto geometry = GeometryParser().parseGeometry(geom_elem.firstChildElement("ShaderInstancedIndexedPrimitives"));

    if(!geometry)
        return {};

    geometry->m_Name = key_str + "_" + name;
    geometry->m_Texture.m_TextureName = m_SetName + ".png";
    geometry->m_Texture.m_TextureOrgFile = m_OrgPath + "/" + m_Name + ".png";

    return std::make_pair(key, *geometry);
}