#include "writer/LevelWriter.h"

#include <QtCore/QFile>
#include <QtCore/QDir>

#include <assimp/Exporter.hpp>
#include <assimp/scene.h>

void LevelWriter::writeLevel(const Level& level)
{
    m_SaveName = level.m_LevelName;

    for(const auto& ao : level.m_ArtObjects)
    {
        const auto& ao_name = ao.m_Name;
        const auto ao_geom_find_iter = level.m_ArtObjectGeometries.find(ao_name);

        if(ao_geom_find_iter == level.m_ArtObjectGeometries.cend())
            continue;

        const auto& ao_geom = *ao_geom_find_iter;

        const auto mesh_id = addGeometry(ao_geom.second);

        if(!mesh_id)
            continue;

        const auto nodes = new aiNode*[1];
        nodes[0] = new aiNode;
        const auto node = nodes[0];

        aiVector3D pos = aiVector3D(ao.m_Position.x(), ao.m_Position.y(), ao.m_Position.z());
        aiVector3D sca = aiVector3D(ao.m_Scale.x(), ao.m_Scale.y(), ao.m_Scale.z());
        aiQuaternion rot = aiQuaternion(ao.m_Rotation.w(), ao.m_Rotation.x(), ao.m_Rotation.y(), ao.m_Rotation.z());
        
        node->mTransformation = aiMatrix4x4(sca, rot, pos);
        
        node->mMeshes = new unsigned int[1];
        node->mMeshes[0] = *mesh_id;
        node->mNumMeshes = 1;

        m_Scene->mRootNode->addChildren(1, nodes);
    }

    save();
}
