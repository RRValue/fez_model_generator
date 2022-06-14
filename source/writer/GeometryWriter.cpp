#include "writer/GeometryWriter.h"

#include <QtCore/QFile>

#include <assimp/Exporter.hpp>
#include <assimp/scene.h>

GeometryWriter::GeometryWriter(const QString& path) : m_Path{path}, m_Scene{new aiScene()}
{
    m_Scene->mNumMeshes = 0;
    m_Scene->mNumMaterials = 0;

    m_Scene->mRootNode = new aiNode();
    m_Scene->mRootNode->mTransformation = aiMatrix4x4({1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1});
    m_Scene->mRootNode->mNumMeshes = 1;
    m_Scene->mRootNode->mMeshes = new unsigned int[1];
    m_Scene->mRootNode->mMeshes[0] = 0;
}

GeometryWriter::~GeometryWriter()
{
    deallocateScene();
}

aiMesh* GeometryWriter::allocateMesh()
{
    const auto old_num_meshes = m_Scene->mNumMeshes;
    const auto new_num_meshes = ++m_Scene->mNumMeshes;
    const auto meshes = new aiMesh*[new_num_meshes];

    if(old_num_meshes != 0)
    {
        memcpy(meshes, m_Scene->mMeshes, sizeof(aiMesh*) * old_num_meshes);

        delete[] m_Scene->mMeshes;
    }

    m_Scene->mMeshes = meshes;

    meshes[new_num_meshes - 1] = new aiMesh();
    meshes[new_num_meshes - 1]->mMaterialIndex = 0;

    return meshes[new_num_meshes - 1];
}

aiMaterial* GeometryWriter::allocateMaterial()
{
    const auto old_num_materials = m_Scene->mNumMaterials;
    const auto new_num_materials = ++m_Scene->mNumMaterials;
    const auto materials = new aiMaterial*[new_num_materials];

    if(old_num_materials != 0)
    {
        memcpy(materials, m_Scene->mMaterials, sizeof(aiMaterial*) * old_num_materials);

        delete[] m_Scene->mMaterials;
    }

    m_Scene->mMaterials = materials;

    materials[new_num_materials - 1] = new aiMaterial();

    return materials[new_num_materials - 1];
}

void GeometryWriter::deallocateScene()
{
    delete m_Scene;
}

void GeometryWriter::writeObj(const Geometry& geometry)
{
    const auto mesh = allocateMesh();
    const auto material = allocateMaterial();

    // load vertices
    const auto& num_vertices = geometry.m_Vertices.size();

    mesh->mNumVertices = (unsigned int)num_vertices;

    if(num_vertices == 0)
        return;

    mesh->mVertices = new aiVector3D[num_vertices];
    mesh->mNormals = new aiVector3D[num_vertices];
    mesh->mTextureCoords[0] = new aiVector3D[num_vertices];

    for(unsigned int i = 0; i < num_vertices; i++)
    {
        const auto& v = geometry.m_Vertices[i];
        const auto& p = v.m_Position;
        const auto& n = v.m_Normal;
        const auto& t = v.m_TextureCoordinate;

        mesh->mVertices[i] = aiVector3D({p[0], p[1], p[2]});
        mesh->mNormals[i] = aiVector3D({n[0], n[1], n[2]});
        mesh->mTextureCoords[0][i] = aiVector3D({t[0], t[1], 0});
    }

    // load faces
    auto num_faces = geometry.m_Indices.size() / 3;

    mesh->mNumFaces = (unsigned int)num_faces;

    if(num_faces == 0)
        return;

    mesh->mFaces = new aiFace[num_faces];

    for(size_t i = 0; i < num_faces; i++)
    {
        auto& ai_face = mesh->mFaces[i];

        ai_face.mNumIndices = 3;
        ai_face.mIndices = new unsigned int[3];

        ai_face.mIndices[0] = geometry.m_Indices[3 * i + 0];
        ai_face.mIndices[1] = geometry.m_Indices[3 * i + 1];
        ai_face.mIndices[2] = geometry.m_Indices[3 * i + 2];
    }

    // load material
    aiString fileName(geometry.m_TextureName.toStdString() + ".png");
    material->AddProperty(&fileName, AI_MATKEY_NAME);
    material->AddProperty(&fileName, AI_MATKEY_TEXTURE(aiTextureType_DIFFUSE, 0));

    // save
    Assimp::Exporter exporter;
    aiReturn success;

    success = exporter.Export(m_Scene, "obj", m_Path.toStdString() + "/" + geometry.m_Name.toStdString() + ".obj");

    if(success != aiReturn_SUCCESS)
    {
        std::string export_error_string(exporter.GetErrorString());

        return;
    }
}
