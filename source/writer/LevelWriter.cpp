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

        aiVector3D pos = aiVector3D(ao.m_Position.x() - 0.5f, ao.m_Position.y() - 0.5f, ao.m_Position.z() - 0.5f);
        aiVector3D sca = aiVector3D(ao.m_Scale.x(), ao.m_Scale.y(), ao.m_Scale.z());
        aiQuaternion rot = aiQuaternion(ao.m_Rotation.w(), ao.m_Rotation.x(), ao.m_Rotation.y(), ao.m_Rotation.z());
        
        node->mTransformation = aiMatrix4x4(sca, rot, pos);
        
        node->mName = ao.m_Name.toStdString();
        node->mMeshes = new unsigned int[1];
        node->mMeshes[0] = *mesh_id;
        node->mNumMeshes = 1;

        m_Scene->mRootNode->addChildren(1, nodes);
    }

    for(const auto& te : level.m_TrileEmplacements)
    {
        const auto& trile_id = te.m_Id;
        const auto trile_geom_find_iter = level.m_TrileGeometries.find(trile_id);

        if(trile_geom_find_iter == level.m_TrileGeometries.cend())
            continue;

        const auto& trile_geom = *trile_geom_find_iter;

        const auto mesh_id = addGeometry(trile_geom.second);

        if(!mesh_id)
            continue;

        const auto nodes = new aiNode*[1];
        nodes[0] = new aiNode;
        const auto node = nodes[0];

        aiVector3D pos = aiVector3D(te.m_Position.x(), te.m_Position.y(), te.m_Position.z());
        aiVector3D sca = aiVector3D(1.0f, 1.0f, 1.0f);
        aiQuaternion rot = [](const auto& orientation) {
            switch(orientation)
            {
                case 0: return aiQuaternion(M_PI, 0.0f, 0.0f);
                case 1: return aiQuaternion(M_PI * -0.5f, 0.0f, 0.0f);
                case 2: return aiQuaternion(0.0f, 0.0f, 0.0f);
                case 3: return aiQuaternion(M_PI * 0.5f, 0.0f, 0.0f);
                default: return aiQuaternion();
            }
        }(te.m_Orintation);

        node->mTransformation = aiMatrix4x4(sca, rot, pos);

        node->mName = QString::number(te.m_Id).toStdString();
        node->mMeshes = new unsigned int[1];
        node->mMeshes[0] = *mesh_id;
        node->mNumMeshes = 1;

        m_Scene->mRootNode->addChildren(1, nodes);
    }

    for(const auto& bp : level.m_BackgroundPlanes)
    {
        const auto mesh_id = addGeometry(bp.m_Geometry);

        if(!mesh_id)
            continue;

        const auto nodes = new aiNode*[1];
        nodes[0] = new aiNode;
        const auto node = nodes[0];

        aiVector3D pos = aiVector3D(bp.m_Position.x() - 0.5f, bp.m_Position.y() - 0.5f, bp.m_Position.z() - 0.5f);
        aiVector3D sca = aiVector3D(bp.m_Scale.x(), bp.m_Scale.y(), bp.m_Scale.z());
        aiQuaternion rot = aiQuaternion(bp.m_Rotation.w(), bp.m_Rotation.x(), bp.m_Rotation.y(), bp.m_Rotation.z());

        node->mTransformation = aiMatrix4x4(sca, rot, pos);

        node->mName = bp.m_Name.toStdString();
        node->mMeshes = new unsigned int[1];
        node->mMeshes[0] = *mesh_id;
        node->mNumMeshes = 1;

        m_Scene->mRootNode->addChildren(1, nodes);
    }

    for(const auto& car : level.m_Characters)
    {
        const auto mesh_id = addGeometry(car.m_Geometry);

        if(!mesh_id)
            continue;

        const auto nodes = new aiNode*[1];
        nodes[0] = new aiNode;
        const auto node = nodes[0];

        aiVector3D pos = aiVector3D(car.m_Position.x() - 0.5f, car.m_Position.y() - 0.5f, car.m_Position.z() - 0.5f);
        aiVector3D sca = aiVector3D(1.0f, 1.0f, 1.0f);
        aiQuaternion rot = aiQuaternion();

        node->mTransformation = aiMatrix4x4(sca, rot, pos);

        node->mName = car.m_Name.toStdString();
        node->mMeshes = new unsigned int[1];
        node->mMeshes[0] = *mesh_id;
        node->mNumMeshes = 1;

        m_Scene->mRootNode->addChildren(1, nodes);
    }

    save();
}
