#include "export/SceneBuilder.h"

#include "geometry/Image.h"

#include <QtGui/QImage>

#include <assimp/material.h>
#include <assimp/scene.h>

namespace fez
{

namespace
{

// Moves the owned objects into one array as aiScene expects it (allocated once with new[], freed by ~aiScene).
template<typename T>
T** releaseAll(std::vector<std::unique_ptr<T>>& owned, unsigned int& count)
{
    count = 0;

    if(owned.empty())
        return nullptr;

    auto array = std::make_unique<T*[]>(owned.size());

    for(size_t i = 0; i < owned.size(); ++i)
        array[i] = owned[i].release();

    count = static_cast<unsigned int>(owned.size());
    owned.clear();

    return array.release();
}

} // namespace

SceneBuilder::SceneBuilder() = default;

SceneBuilder::~SceneBuilder() = default;

std::expected<SceneBuilder::MeshId, Error> SceneBuilder::addMesh(const Geometry& geometry)
{
    const auto num_vertices = geometry.m_Vertices.size();
    const auto num_faces = geometry.m_Indices.size() / 3;

    // empty geometry: no mesh and no material
    if(num_vertices == 0)
        return std::unexpected(Error{ErrorKind::BadValue, "geometry has no vertices", geometry.m_Name.toStdString()});

    if(num_faces == 0)
        return std::unexpected(Error{ErrorKind::BadValue, "geometry has no faces", geometry.m_Name.toStdString()});

    // one mesh and one material per call
    const auto mesh_id = static_cast<MeshId>(m_Meshes.size());
    const auto material_index = static_cast<unsigned int>(m_Materials.size());

    const auto mesh = m_Meshes.emplace_back(std::make_unique<aiMesh>()).get();
    const auto material = m_Materials.emplace_back(std::make_unique<aiMaterial>()).get();

    // load vertices
    mesh->mNumVertices = static_cast<unsigned int>(num_vertices);
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

    // load faces, winding (0, 2, 1)
    mesh->mNumFaces = static_cast<unsigned int>(num_faces);
    mesh->mFaces = new aiFace[num_faces];

    for(size_t i = 0; i < num_faces; i++)
    {
        auto& ai_face = mesh->mFaces[i];

        ai_face.mNumIndices = 3;
        ai_face.mIndices = new unsigned int[3];

        ai_face.mIndices[0] = static_cast<unsigned int>(geometry.m_Indices[3 * i + 0]);
        ai_face.mIndices[1] = static_cast<unsigned int>(geometry.m_Indices[3 * i + 2]);
        ai_face.mIndices[2] = static_cast<unsigned int>(geometry.m_Indices[3 * i + 1]);
    }

    mesh->mName = geometry.m_Name.toStdString();

    // load material
    aiString material_name(geometry.m_Texture.m_TextureName.toStdString());
    aiString diffuse_texture_filename(geometry.m_Texture.m_TextureName.toStdString());

    material->AddProperty(&material_name, AI_MATKEY_NAME);
    material->AddProperty(&diffuse_texture_filename, AI_MATKEY_TEXTURE_DIFFUSE(0));

    const auto opacity = geometry.m_Opacity;
    const auto double_sided = geometry.m_DoubleSided;

    material->AddProperty(&opacity, 1, AI_MATKEY_OPACITY);
    material->AddProperty(&double_sided, 1, AI_MATKEY_TWOSIDED);

    // test on transparency
    if(geometry.m_IsPlane)
    {
        QImage texture(geometry.m_Texture.m_TextureOrgFile);

        if(texture.isNull())
            return std::unexpected(Error{ErrorKind::FileNotFound, "plane texture cannot be loaded",
                                         geometry.m_Texture.m_TextureOrgFile.toStdString()});

        if(hasTranslucentPixels(texture))
        {
            aiString opacity_texture_filename(geometry.m_Texture.m_TextureName.toStdString());

            material->AddProperty(&opacity_texture_filename, AI_MATKEY_TEXTURE_OPACITY(0));
        }
    }

    mesh->mMaterialIndex = material_index;

    return mesh_id;
}

void SceneBuilder::addNode(const QString& name, const Transform& transform, MeshId meshId)
{
    auto node = std::make_unique<aiNode>();

    const auto& p = transform.m_Position;
    const auto& r = transform.m_Rotation;
    const auto& s = transform.m_Scale;

    const aiVector3D pos(p.x(), p.y(), p.z());
    const aiVector3D sca(s.x(), s.y(), s.z());
    const aiQuaternion rot(r.w(), r.x(), r.y(), r.z());

    node->mTransformation = aiMatrix4x4(sca, rot, pos);

    node->mName = name.toStdString();
    node->mMeshes = new unsigned int[1];
    node->mMeshes[0] = meshId;
    node->mNumMeshes = 1;

    m_Nodes.push_back(std::move(node));
}

std::unique_ptr<aiScene> SceneBuilder::build()
{
    auto scene = std::make_unique<aiScene>();

    scene->mRootNode = new aiNode();
    scene->mRootNode->mTransformation = aiMatrix4x4({1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1});

    scene->mMeshes = releaseAll(m_Meshes, scene->mNumMeshes);
    scene->mMaterials = releaseAll(m_Materials, scene->mNumMaterials);

    if(!m_Nodes.empty())
    {
        std::vector<aiNode*> children;
        children.reserve(m_Nodes.size());

        for(auto& node : m_Nodes)
            children.push_back(node.release());

        m_Nodes.clear();

        scene->mRootNode->addChildren(static_cast<unsigned int>(children.size()), children.data());
    }

    return scene;
}

} // namespace fez
