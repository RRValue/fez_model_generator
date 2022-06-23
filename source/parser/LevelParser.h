#pragma once

#include "model/Level.h"
#include "model/Texture.h"

#include <QtCore/QString>
#include <QtCore/QMutex>

#include <QtXml/QDomDocument>

class LevelParser
{
    using LevelResult = std::optional<Level>;
    
    using TrileEmplacementsResult = std::optional<Level::TrileEmplacements>;
    using TrileGeometriesResult = std::optional<Level::TrileGeometries>;

    using ArtObjectsResult = std::optional<Level::ArtObjects>;
    using ArtObjectGeometriesResult = std::optional<Level::ArtObjectGeometries>;

    using BackgroundPlanesResult = std::optional<Level::BackgroundPlanes>;
    using TextureResult = std::optional<Texture>;

    using TrileSetCache = std::map<QString, Level::TrileGeometries>;
    using ArtObjectCache = std::map<QString, Geometry>;
    using TextureCache = std::map<QString, Texture>;

public:
    LevelParser();
    ~LevelParser();

    LevelResult parse(const QString& path) noexcept;

private:
    TrileEmplacementsResult readTrileEmplacements(const QDomElement& elem);
    ArtObjectsResult readArtObjects(const QDomElement& elem);
    BackgroundPlanesResult readBackgroundPlanes(const QDomElement& elem);

    TrileGeometriesResult parseTrileEmplacements(const Level::TrileEmplacements& emplacements, const QString& trileSetName);
    ArtObjectGeometriesResult parseArtObjects(const Level::ArtObjects& artObjects);
    BackgroundPlanesResult parseBackgroundPlanes(const Level::BackgroundPlanes& backgroundPlanes);

private:
    QDomDocument m_Document;

    QString m_Path;

    static QMutex sm_TrileSetCacheMutex;
    static TrileSetCache sm_TrileSetCache;

    static QMutex sm_ArtObjectCacheMutex;
    static ArtObjectCache sm_ArtObjectCache;

    static QMutex sm_TextureCacheMutex;
    static TextureCache sm_TextureCache;
};
