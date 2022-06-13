#include "parser/ArtObjectParser.h"

#include <QtCore/QFile>
#include <QtCore/QFileInfo>
#include <QtCore/QDir>

#include <assimp/scene.h>
#include <assimp/Exporter.hpp>

#include <qdebug.h>

ArtObjectParser::ArtObjectParser() : m_Document{}, m_Scene{new aiScene()}
{
    m_Scene->mNumMeshes = 0;
    m_Scene->mNumMaterials = 0;

    m_Scene->mRootNode = new aiNode();
    m_Scene->mRootNode->mTransformation = aiMatrix4x4({1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1});
    m_Scene->mRootNode->mNumMeshes = 1;
    m_Scene->mRootNode->mMeshes = new unsigned int[1];
    m_Scene->mRootNode->mMeshes[0] = 0;
}

ArtObjectParser::~ArtObjectParser()
{
    deallocateScene();
}

aiMesh* ArtObjectParser::allocateMesh()
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

aiMaterial* ArtObjectParser::allocateMaterial()
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

void ArtObjectParser::deallocateScene()
{
    // delete meshes interna
    for(size_t i = 0; i < m_Scene->mNumMeshes; i++)
        delete m_Scene->mMeshes[i];

    // delete array of meshes
    delete[] m_Scene->mMeshes;

    // delete materials interna
    for(size_t i = 0; i < m_Scene->mNumMaterials; i++)
        delete m_Scene->mMaterials[i];

    // delete array of materials
    delete[] m_Scene->mMaterials;

    // delete root node
    delete m_Scene->mRootNode;
}

void ArtObjectParser::parse(const QString& path) noexcept
{
    QFileInfo info(path);

    if(!info.exists())
        return;

    static const auto out_folder_name = "exported";

    m_Name = info.baseName();
    m_Path = info.absolutePath();
    m_Document.clear();

    QDir dir(m_Path);

    if(!dir.exists(out_folder_name))
        dir.mkdir(out_folder_name);

    m_Path = m_Path + "/" + out_folder_name;

    // load
    auto xml_file = QFile(path);

    if(!xml_file.exists())
        return;

    if(!xml_file.open(QIODevice::OpenModeFlag::ReadOnly))
        return;

    QString error_msg;
    int error_line;
    int error_column;

    if(!m_Document.setContent(&xml_file, &error_msg, &error_line, &error_column))
    {
        qDebug() << "Error: \"" + error_msg + "\" at line: " + QString::number(error_line) + " Columns: " + QString::number(error_column);

        return;
    }

    // parse
    const auto art_obj_elem = m_Document.firstChildElement("ArtObject");

    if(art_obj_elem.isNull())
        return;

    const auto mesh = allocateMesh();
    const auto material = allocateMaterial();

    if(!m_GeomParser.parseGeometry(art_obj_elem.firstChildElement("ShaderInstancedIndexedPrimitives"), mesh, material, m_Name))
        return;

    writeObj();
}

void ArtObjectParser::writeObj()
{
    // save
    Assimp::Exporter exporter;
    aiReturn success;

    success = exporter.Export(m_Scene, "obj", m_Path.toStdString() + "/" + m_Name.toStdString() + ".obj");

    if(success != aiReturn_SUCCESS)
    {
        std::string export_error_string(exporter.GetErrorString());

        return;
    }

    QFile texture(m_Path + "/../" + m_Name + ".png");
    texture.copy(m_Path + "/" + m_Name + ".png");
}