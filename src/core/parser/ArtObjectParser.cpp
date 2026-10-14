#include "parser/ArtObjectParser.h"

#include "log/Log.h"
#include "parser/GeometryParser.h"
#include "xml/Xml.h"

#include <QtCore/QFileInfo>

namespace fez
{

std::expected<Geometry, Error> parseArtObject(const QString& path)
{
    log::debug("parsing: {}", path);

    const QFileInfo info(path);
    const auto name = info.baseName();
    const auto org_path = info.absolutePath();

    const auto document = loadXml(path);

    if(!document)
    {
        if(document.error().kind == ErrorKind::XmlSyntax)
            log::debug("Error: \"{}\" at {}", document.error().message, document.error().source);

        return std::unexpected(document.error());
    }

    // QDomDocument::firstChildElement: the document element, if it is named ArtObject
    const auto art_obj_elem = document->firstChildElement("ArtObject");

    if(art_obj_elem.isNull())
        return std::unexpected(Error{ErrorKind::MissingElement, "missing element 'ArtObject'", "ArtObject"});

    const auto primitives_elem = childElement(art_obj_elem, "ShaderInstancedIndexedPrimitives");

    if(!primitives_elem)
        return std::unexpected(primitives_elem.error());

    auto geometry = parseGeometry(*primitives_elem);

    if(!geometry)
        return std::unexpected(geometry.error());

    geometry->m_Name = name;
    geometry->m_Texture.m_TextureName = name + ".png";
    geometry->m_Texture.m_TextureOrgFile = org_path + "/" + name + ".png";

    return geometry;
}

} // namespace fez
