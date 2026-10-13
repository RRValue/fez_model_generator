#pragma once

#include "math/Quaternion.h"
#include "math/Vector.h"
#include "model/ArtObject.h"
#include "model/TrileEmplacement.h"

#include <QtCore/QString>

#include <utility>
#include <vector>

namespace fez
{

// A background plane as written in the level XML (no texture loaded, no quad built).
struct BackgroundPlaneDescription
{
    QString m_TextureName;
    bool m_Animated = false;
    float m_Opacity = 1.0f;
    bool m_DoubleSided = false;
    Vec3f m_Position = Vec3f::Zero();
    QuaternionF m_Rotation = QuaternionF::Identity();
    Vec3f m_Scale = Vec3f::Zero();
};

// A non-player character as written in the level XML (no texture loaded, no quad built).
struct NpcDescription
{
    using Actions = std::vector<std::pair<QString, QString>>;  // action key, animation name

    QString m_Name;
    Actions m_Actions = {};
    Vec3f m_Position = Vec3f::Zero();
};

// Everything a level XML says, before any referenced file (trile set, art object, texture) is loaded.
struct LevelDescription
{
    using TrileEmplacements = std::vector<TrileEmplacement>;
    using ArtObjects = std::vector<ArtObject>;
    using BackgroundPlanes = std::vector<BackgroundPlaneDescription>;
    using Npcs = std::vector<NpcDescription>;

    QString m_Name = {};
    QString m_TrileSetName = {};
    TrileEmplacements m_TrileEmplacements = {};
    ArtObjects m_ArtObjects = {};
    BackgroundPlanes m_BackgroundPlanes = {};  // read from Task 19 on
    Npcs m_Npcs = {};                          // read from Task 19 on
};

} // namespace fez
