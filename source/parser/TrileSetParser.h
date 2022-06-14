#pragma once

#include "geom/Geometry.h"

#include <QtCore/QString>

#include <QtXml/QDomDocument>

class TrileSetParser
{
    using GeometryResult = std::optional<Geometry>;
    using GeometryResults = std::list<Geometry>;

public:
    TrileSetParser();
    ~TrileSetParser();

    GeometryResults parse(const QString& path) noexcept;

private:
    GeometryResult parserTrile(const QDomElement& elem);

private:
    QDomDocument m_Document;

    QString m_OrgPath;
    QString m_SetName;
    QString m_Name;
    QString m_OutPath;
};