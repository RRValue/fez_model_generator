#pragma once

#include "math/Vector.h"
#include "math/Quaternion.h"

#include <QtCore/QString>

#include <QtXml/QDomDocument>

struct aiMesh;
struct aiMaterial;

class LevelParser
{
    struct TrileEmplacement
    {
        int m_Id = 0;
        Vec3f m_Emplacement = Vec3f::Zero();
        Vec3f m_Position = Vec3f::Zero();
        unsigned int m_Orintation = 0;
    };

    struct ArtObject
    {
        QString m_Name;
        Vec3f m_Position = Vec3f::Zero();
        QuaternionF m_Rotation = QuaternionF::Identity();
        Vec3f m_Scale= Vec3f::Zero();
    };

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
