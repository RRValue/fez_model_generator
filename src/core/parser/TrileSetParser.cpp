#include "parser/TrileSetParser.h"

#include "parser/GeometryParser.h"
#include "xml/Xml.h"

#include <QtCore/QFileInfo>

#include <qdebug.h>

#include <utility>

namespace fez
{

namespace
{

struct TrileFiles
{
    QString m_SetName;
    QString m_OrgPath;
    QString m_BaseName;
};

std::expected<std::pair<int, Geometry>, Error> parseTrile(const QDomElement& entry_elem, const TrileFiles& files)
{
    const auto key_str = attribute<QString>(entry_elem, "key");

    if(!key_str)
        return std::unexpected(key_str.error());

    const auto trile_elem = childElement(entry_elem, "Trile");

    qDebug() << "\t: " << *key_str;

    if(!trile_elem)
        return std::unexpected(trile_elem.error());

    const auto key = attribute<int>(entry_elem, "key");

    if(!key)
        return std::unexpected(key.error());

    const auto name = attribute<QString>(*trile_elem, "name");

    if(!name)
        return std::unexpected(name.error());

    const auto geom_elem = childElement(*trile_elem, "Geometry");

    if(!geom_elem)
        return std::unexpected(geom_elem.error());

    const auto primitives_elem = childElement(*geom_elem, "ShaderInstancedIndexedPrimitives");

    if(!primitives_elem)
        return std::unexpected(primitives_elem.error());

    auto geometry = parseGeometry(*primitives_elem);

    if(!geometry)
        return std::unexpected(geometry.error());

    geometry->m_Name = *key_str + "_" + *name;
    geometry->m_Texture.m_TextureName = files.m_SetName + ".png";
    geometry->m_Texture.m_TextureOrgFile = files.m_OrgPath + "/" + files.m_BaseName + ".png";

    return std::make_pair(*key, std::move(*geometry));
}

} // namespace

std::expected<TrileSet, Error> parseTrileSet(const QString& path)
{
    qDebug() << "parsing: " << path;

    const QFileInfo info(path);

    const auto document = loadXml(path);

    if(!document)
    {
        if(document.error().kind == ErrorKind::XmlSyntax)
            qDebug() << "Error: \"" + QString::fromStdString(document.error().message) + "\" at " + QString::fromStdString(document.error().source);

        return std::unexpected(document.error());
    }

    // QDomDocument::firstChildElement: the document element, if it is named TrileSet
    const auto trile_set_elem = document->firstChildElement("TrileSet");

    if(trile_set_elem.isNull())
        return std::unexpected(Error{ErrorKind::MissingElement, "missing element 'TrileSet'", "TrileSet"});

    const auto set_name = attribute<QString>(trile_set_elem, "name");

    if(!set_name)
        return std::unexpected(set_name.error());

    const auto triles_elem = childElement(trile_set_elem, "Triles");

    if(!triles_elem)
        return std::unexpected(triles_elem.error());

    const TrileFiles files{*set_name, info.absolutePath(), info.baseName()};

    TrileSet result;
    result.m_Name = *set_name;

    // From the first TrileEntry on, every following sibling element is an entry (no name filter, as before).
    const auto first_entry = triles_elem->firstChildElement("TrileEntry");

    for(auto iter = ChildElementRange::Iterator(first_entry, QString()); iter != ChildElementRange::Iterator(); ++iter)
    {
        auto trile = parseTrile(*iter, files);

        if(!trile)
            return std::unexpected(trile.error());

        result.m_Triles.insert(std::move(*trile));
    }

    return result;
}

} // namespace fez
