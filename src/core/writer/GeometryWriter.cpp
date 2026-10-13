#include "writer/GeometryWriter.h"

#include <QtCore/QFile>
#include <QtCore/QDir>

#include <assimp/Exporter.hpp>
#include <assimp/scene.h>

void GeometryWriter::writeObj(const Geometry& geometry)
{
    m_SaveName = geometry.m_Name;
    const auto mesh_id = addGeometry(geometry);

    if(!mesh_id)
        return;

    const auto nodes = new aiNode*[1];
    nodes[0] = new aiNode;
    const auto node = nodes[0];

    aiVector3D pos = aiVector3D(0.0f, 0.0f, 0.0f);
    aiVector3D sca = aiVector3D(1.0f, 1.0f, 1.0f);
    aiQuaternion rot;

    node->mTransformation = aiMatrix4x4(sca, rot, pos);

    node->mMeshes = new unsigned int[1];
    node->mMeshes[0] = *mesh_id;
    node->mNumMeshes = 1;

    m_Scene->mRootNode->addChildren(1, nodes);

    save();
}
