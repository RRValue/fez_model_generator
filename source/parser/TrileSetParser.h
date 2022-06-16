#pragma once

#include "model/Geometry.h"

#include <QtCore/QString>

#include <QtXml/QDomDocument>

class TrileSetParser
{
    using TrileResult = std::optional<std::pair<int, Geometry>>;
    using GeometryResults = std::map<int, Geometry>;

public:
    TrileSetParser();
    ~TrileSetParser();

    GeometryResults parse(const QString& path) noexcept;

private:
    TrileResult parserTrile(const QDomElement& elem);

private:
    QDomDocument m_Document;

    QString m_OrgPath;
    QString m_SetName;
    QString m_Name;
    QString m_OutPath;
};