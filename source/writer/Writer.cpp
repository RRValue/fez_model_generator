#include "writer/Writer.h"

#include <QtCore/QFile>
#include <QtCore/QDir>

#include <QtGui/QImage>

#include <assimp/Exporter.hpp>
#include <assimp/scene.h>

Writer::Writer(const QString& path) : m_Path{path}, m_Scene{new aiScene()}
{
    m_Scene->mNumMeshes = 0;
    m_Scene->mNumMaterials = 0;

    m_Scene->mRootNode = new aiNode();
    m_Scene->mRootNode->mTransformation = aiMatrix4x4({1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1});

    // make path
    QDir dir(m_Path);

    if(!dir.exists())
        dir.mkpath(".");
}

Writer::~Writer()
{
    deallocateScene();
}

Writer::MeshAllocation Writer::allocateMesh()
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

    meshes[old_num_meshes] = new aiMesh();

    return {old_num_meshes, meshes[old_num_meshes]};
}

Writer::MaterialAllocation Writer::allocateMaterial()
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

    materials[old_num_materials] = new aiMaterial();

    return {old_num_materials, materials[old_num_materials]};
}

void Writer::deallocateScene()
{
    delete m_Scene;
}

Writer::MeshId Writer::addGeometry(const Geometry& geometry)
{
    const auto mesh_allocation = allocateMesh();
    const auto material_allocation = allocateMaterial();

    const auto mesh = mesh_allocation.second;
    const auto material = material_allocation.second;

    // load vertices
    const auto& num_vertices = geometry.m_Vertices.size();

    mesh->mNumVertices = (unsigned int)num_vertices;

    if(num_vertices == 0)
        return {};

    mesh->mVertices = new aiVector3D[num_vertices];
    mesh->mNormals = new aiVector3D[num_vertices];
    mesh->mTextureCoords[0] = new aiVector3D[num_vertices];
    mesh->mPrimitiveTypes = aiPrimitiveType::aiPrimitiveType_TRIANGLE;

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
        return {};

    mesh->mFaces = new aiFace[num_faces];

    for(size_t i = 0; i < num_faces; i++)
    {
        auto& ai_face = mesh->mFaces[i];

        ai_face.mNumIndices = 3;
        ai_face.mIndices = new unsigned int[3];

        ai_face.mIndices[0] = geometry.m_Indices[3 * i + 0];
        ai_face.mIndices[1] = geometry.m_Indices[3 * i + 2];
        ai_face.mIndices[2] = geometry.m_Indices[3 * i + 1];
    }

    mesh->mName = geometry.m_Name.toStdString();
    
    // load material
    aiString material_name(geometry.m_Texture.m_TextureName.toStdString());
    aiString diffuse_texture_filename(geometry.m_Texture.m_TextureName.toStdString());
    aiString opacity_texture_filename(geometry.m_Texture.m_TextureName.toStdString());
    
    auto result = material->AddProperty(&material_name, AI_MATKEY_NAME);
    result = material->AddProperty(&diffuse_texture_filename, AI_MATKEY_TEXTURE_DIFFUSE(0));

    const auto opacity = geometry.m_Opacity;
    const auto double_sided = geometry.m_DoubleSided;

    result = material->AddProperty(&opacity, 1, AI_MATKEY_OPACITY);
    result = material->AddProperty(&double_sided, 1, AI_MATKEY_TWOSIDED);

    // test on transparency
    if(geometry.m_IsBackgroundPlane)
    {
        QImage texture(geometry.m_Texture.m_TextureOrgFile);

        if(texture.isNull())
            return {};
        
        if(texture.hasAlphaChannel())
        {
            bool use_opacity = false;

            for(int x = 0; x < texture.width() && !use_opacity; x++)
                for(int y = 0; y < texture.height() && !use_opacity; y++)
                    use_opacity = texture.pixelColor(x, y).alphaF() < 1.0f;

            if(use_opacity)
                result = material->AddProperty(&opacity_texture_filename, AI_MATKEY_TEXTURE_OPACITY(0));
        }    

    }

    m_Textures.push_back(std::make_pair(geometry.m_Texture.m_TextureOrgFile, m_Path + "/" + geometry.m_Texture.m_TextureName));

    mesh->mMaterialIndex = material_allocation.first;

    return mesh_allocation.first;
}

void Writer::save()
{
    // save
    Assimp::Exporter exporter;
    aiReturn success = exporter.Export(m_Scene, "obj", m_Path.toStdString() + "/" + m_SaveName.toStdString() + ".obj");
    
    if(success != aiReturn_SUCCESS)
    {
        std::string export_error_string(exporter.GetErrorString());

        return;
    }

    for(const auto& t: m_Textures)
    {
        const auto texture_out_dir = QDir(QFileInfo(t.second).absolutePath());

        if(!texture_out_dir.exists())
            texture_out_dir.mkdir(".");

        QFile texture(t.first);
        texture.copy(t.second);
    }
}