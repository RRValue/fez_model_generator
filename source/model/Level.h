#pragma once

#include "model/TrileEmplacement.h"
#include "model/ArtObject.h"
#include "model/Geometry.h"

struct Level
{
    using TrileEmplacements = std::vector<TrileEmplacement>;
    using TrileGeometries = std::map<int, Geometry>;

    using ArtObjects = std::vector<ArtObject>;
    using ArtObjectGeometries = std::map<QString, Geometry>;

    QString m_TrileSetName;
    TrileEmplacements m_TrileEmplacements;
    TrileGeometries m_TrileGeometries;
    
    ArtObjects m_ArtObjects;
    ArtObjectGeometries m_ArtObjectGeometries;
};