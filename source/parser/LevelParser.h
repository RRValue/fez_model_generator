#pragma once

#include "model/TrileEmplacement.h"
#include "model/ArtObject.h"

#include <QtCore/QString>

#include <QtXml/QDomDocument>

struct aiMesh;
struct aiMaterial;

class LevelParser
{
    using TrileEmplacements = std::vector<TrileEmplacement>;
    using ArtObjects = std::vector<ArtObject>;

public:
    LevelParser();
    ~LevelParser();

    void parse(const QString& path) noexcept;

private:
    TrileEmplacements readTrileEmplacements(const QDomElement& elem);
    ArtObjects readArtObjects(const QDomElement& elem);

private:
    QDomDocument m_Document;

    QString m_TrileSetName;
    TrileEmplacements m_TrileEmplacements;
    ArtObjects m_ArtObjects;
};
