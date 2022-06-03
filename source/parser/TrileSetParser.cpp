#include "parser/TrileSetParser.h"

#include <QtCore/QFile>
#include <QtCore/QFileInfo>
#include <QtCore/QDir>

#include <assimp/scene.h>
#include <assimp/Exporter.hpp>

#include <qdebug.h>

TrileSetParser::TrileSetParser() : m_Document{}
{
}

TrileSetParser::~TrileSetParser()
{
}

void TrileSetParser::parse(const QString& path) noexcept
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
    const auto trile_set_elem = m_Document.firstChildElement("TrileSet");

    if(trile_set_elem.isNull())
        return;

    if(!trile_set_elem.hasAttribute("name"))
        return;

    m_SetName = trile_set_elem.attribute("name");

    QDir set_dir(m_Path);

    if(!set_dir.exists(m_SetName))
        set_dir.mkdir(m_SetName);

    const auto triles_elem = trile_set_elem.firstChildElement("Triles");

    if(triles_elem.isNull())
        return;

    // count vertices
    auto trile_entry_elem = triles_elem.firstChildElement("TrileEntry");

    while(!trile_entry_elem.isNull())
    {
        if(!trile_entry_elem.hasAttribute("key"))
            return;

        m_Key = trile_entry_elem.attribute("key");

        const auto trile_elem = trile_entry_elem.firstChildElement("Trile");

        if(trile_elem.isNull())
            return;

        const auto geom_elem = trile_elem.firstChildElement("Geometry");

        if(geom_elem.isNull())
            return;

        const auto geometry = m_GeomParser.parseGeometry(geom_elem.firstChildElement("ShaderInstancedIndexedPrimitives"));

        if(!geometry)
            return;

        writeObj(*geometry);

        trile_entry_elem = trile_entry_elem.nextSiblingElement();
    }
}

void TrileSetParser::writeObj(const Geometry& geometry)
{
    // allocate
    aiScene* scene = new aiScene();

    scene->mNumMaterials = 1;
    scene->mMaterials = new aiMaterial*[scene->mNumMaterials];
    scene->mMaterials[0] = new aiMaterial();

    scene->mNumMeshes = 1;
    scene->mMeshes = new aiMesh*[scene->mNumMeshes];
    scene->mMeshes[0] = new aiMesh();
    scene->mMeshes[0]->mMaterialIndex = 0;

    // load vertices
    const auto& num_vertices = geometry.m_Vertices.size();

    scene->mMeshes[0]->mNumVertices = (unsigned int)num_vertices;

    if(num_vertices == 0)
        return;

    scene->mMeshes[0]->mVertices = new aiVector3D[num_vertices];
    scene->mMeshes[0]->mNormals = new aiVector3D[num_vertices];
    scene->mMeshes[0]->mTextureCoords[0] = new aiVector3D[num_vertices];

    for(unsigned int i = 0; i < num_vertices; i++)
    {
        const auto& v = geometry.m_Vertices[i];
        const auto& p = v.m_Position;
        const auto& n = v.m_Normal;
        const auto& t = v.m_TextureCoordinate;

        scene->mMeshes[0]->mVertices[i] = aiVector3D({p[0], p[1], p[2]});
        scene->mMeshes[0]->mNormals[i] = aiVector3D({n[0], n[1], n[2]});
        scene->mMeshes[0]->mTextureCoords[0][i] = aiVector3D({t[0], t[1], 0});
    }

    //load faces
    auto num_faces = geometry.m_Indices.size() / 3;

    scene->mMeshes[0]->mNumFaces = (unsigned int)num_faces;

    if(num_faces == 0)
        return;

    scene->mMeshes[0]->mFaces = new aiFace[num_faces];

    for(size_t i = 0; i < num_faces; i++)
    {
        auto& ai_face = scene->mMeshes[0]->mFaces[i];

        ai_face.mNumIndices = 3;
        ai_face.mIndices = new unsigned int[3];

        ai_face.mIndices[0] = geometry.m_Indices[3 * i + 0];
        ai_face.mIndices[1] = geometry.m_Indices[3 * i + 2];
        ai_face.mIndices[2] = geometry.m_Indices[3 * i + 1];
    }

    // load material
    aiString fileName(m_Name.toStdString() + ".png");
    scene->mMaterials[0]->AddProperty(&fileName, AI_MATKEY_NAME);
    scene->mMaterials[0]->AddProperty(&fileName, AI_MATKEY_TEXTURE(aiTextureType_DIFFUSE, 0));
    
    // finish
    scene->mRootNode = new aiNode();
    scene->mRootNode->mTransformation = aiMatrix4x4({1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1});
    scene->mRootNode->mNumMeshes = 1;
    scene->mRootNode->mMeshes = new unsigned int[1];
    scene->mRootNode->mMeshes[0] = 0;

    // save
    Assimp::Exporter exporter;
    aiReturn success;

    success = exporter.Export(scene, "obj", m_Path.toStdString() + "/" + m_SetName.toStdString() + "/" + m_Key.toStdString() + ".obj");

    if(success != aiReturn_SUCCESS)
    {
        std::string export_error_string(exporter.GetErrorString());

        return;
    }

    QFile texture(m_Path + "/../" + m_Name + ".png");
    texture.copy(m_Path + "/" + m_SetName + "/" + m_Name + ".png");

    // clear
    delete[] scene->mRootNode->mMeshes;
    scene->mRootNode->mMeshes = nullptr;

    // delete root node
    delete scene->mRootNode;
    scene->mRootNode = nullptr;

    // delete meshes interna
    for(size_t i = 0; i < scene->mNumMeshes; i++)
    {
        // delete faces interna
        for(size_t j = 0; j < scene->mMeshes[i]->mNumFaces; j++)
        {
            delete[] scene->mMeshes[i]->mFaces[j].mIndices;
            scene->mMeshes[i]->mFaces[j].mIndices = nullptr;
        }

        // delete faces
        delete[] scene->mMeshes[i]->mFaces;
        scene->mMeshes[i]->mFaces = nullptr;

        // delete normals
        delete[] scene->mMeshes[i]->mNormals;
        scene->mMeshes[i]->mNormals = nullptr;

        // delete vertices
        delete[] scene->mMeshes[i]->mVertices;
        scene->mMeshes[i]->mVertices = nullptr;

        unsigned int numColorChannels = scene->mMeshes[i]->GetNumColorChannels();
        // delete vertex colors
        for(unsigned int i = 0; i < numColorChannels; i++)
        {
            delete[] scene->mMeshes[i]->mColors[i];
            scene->mMeshes[i]->mColors[i] = nullptr;
        }

        unsigned int numUVChannels = scene->mMeshes[i]->GetNumUVChannels();
        // delete texture coords
        for(unsigned int i = 0; i < numUVChannels; i++)
        {
            delete[] scene->mMeshes[i]->mTextureCoords[i];
            scene->mMeshes[i]->mTextureCoords[i] = nullptr;
        }

        // delete mesh
        delete scene->mMeshes[i];
    }

    // delete array of meshes
    delete[] scene->mMeshes;
    scene->mMeshes = nullptr;
    scene->mNumMeshes = 0;

    // delete materials interna
    for(size_t i = 0; i < scene->mNumMaterials; i++)
    {
        // delete material
        delete scene->mMaterials[i];
        scene->mMaterials[i] = nullptr;
    }
    scene->mNumMaterials = 0;

    // delete array of materials
    delete[] scene->mMaterials;
    scene->mMaterials = nullptr;
}