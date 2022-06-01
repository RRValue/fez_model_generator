#pragma once

#include <QtCore/QString>

#include <QtXml/QDomDocument>

class ArtObjectParser
{
public:
    ArtObjectParser();
    ~ArtObjectParser();

    void parse(const QString& path) noexcept;

private:
    QDomDocument m_Document;
};