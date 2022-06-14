#pragma once

#include "geom/Geometry.h"
#include "parser/GeometryParser.h"

#include <QtCore/QString>

#include <QtXml/QDomDocument>

class TrileSetParser
{
public:
    TrileSetParser();
    ~TrileSetParser();

    void parse(const QString& path) noexcept;

private:
    void parserTrile(const QDomElement& elem);

private:
    QDomDocument m_Document;
};