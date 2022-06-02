#include "ArtObjectParser.h"

#include <QtCore/QFile>
#include <QtCore/QFileInfo>
#include <QtCore/QDir>

#include <assimp/scene.h>
#include <assimp/Exporter.hpp>

#include <qdebug.h>

ArtObjectParser::ArtObjectParser() : m_Document{}
{
}

ArtObjectParser::~ArtObjectParser()
{
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

    const auto geometry = parseGeometry(art_obj_elem.firstChildElement("ShaderInstancedIndexedPrimitives"));

    if(!geometry)
        return;

    writeObj(*geometry);
}

ArtObjectParser::GeometryResult ArtObjectParser::parseGeometry(const QDomElement& elem)
{
    if(elem.isNull())
        return {};

    auto vertices = parseVertices(elem.firstChildElement("Vertices"));

    if(!vertices)
        return {};

    auto indices = parseIndices(elem.firstChildElement("Indices"));

    if(!indices)
        return {};

    Geometry result;
    result.m_Vertices = std::move(*vertices);
    result.m_Indices = std::move(*indices);

    return result;
}

ArtObjectParser::VerticesResult ArtObjectParser::parseVertices(const QDomElement& elem)
{
    if(elem.isNull())
        return {};

    VerticesResult::value_type result;

    // count vertices
    auto vertex_elem = elem.firstChildElement("VertexPositionNormalTextureInstance");
    auto vertex_count = size_t(0);

    while(!vertex_elem.isNull())
    {
        vertex_count++;

        vertex_elem = vertex_elem.nextSiblingElement();
    }

    result.resize(vertex_count);

    // set vertices
    vertex_elem = elem.firstChildElement("VertexPositionNormalTextureInstance");
    vertex_count = size_t(0);

    while(!vertex_elem.isNull())
    {
        const auto position_elem = vertex_elem.firstChildElement("Position");
        const auto normal_elem = vertex_elem.firstChildElement("Normal");
        const auto texture_coord_elem = vertex_elem.firstChildElement("TextureCoord");

        if(position_elem.isNull() || normal_elem.isNull() || texture_coord_elem.isNull())
            return {};

        auto& vertex = result[vertex_count];

        // position
        const auto vec3_elem = position_elem.firstChildElement("Vector3");

        if(vec3_elem.isNull())
            return {};

        if(!vec3_elem.hasAttribute("x") || !vec3_elem.hasAttribute("y") || !vec3_elem.hasAttribute("z"))
            return {};

        auto x_ok = false;
        auto y_ok = false;
        auto z_ok = false;

        vertex.m_Position.x() = vec3_elem.attribute("x").toFloat(&x_ok);
        vertex.m_Position.y() = vec3_elem.attribute("y").toFloat(&y_ok);
        vertex.m_Position.z() = vec3_elem.attribute("z").toFloat(&z_ok);

        if(!x_ok || !y_ok || !z_ok)
            return {};

        // normal
        auto normal_index_ok = false;
        const auto normal_index = normal_elem.text().toInt(&normal_index_ok);

        if(!normal_index_ok)
            return {};

        switch(normal_index)
        {
            case 0: vertex.m_Normal = {0.0f, 0.0f, 0.0f}; break;
            case 1: vertex.m_Normal = {1.0f, 1.0f, 1.0f}; break;
            case 2: vertex.m_Normal = {2.0f, 2.0f, 2.0f}; break;
            case 3: vertex.m_Normal = {3.0f, 3.0f, 3.0f}; break;
            case 4: vertex.m_Normal = {4.0f, 4.0f, 4.0f}; break;
            case 5: vertex.m_Normal = {5.0f, 5.0f, 5.0f}; break;
            default: return {};
        }

        // texture coordinate
        const auto vec2_elem = texture_coord_elem.firstChildElement("Vector2");

        if(vec2_elem.isNull())
            return {};

        if(!vec2_elem.hasAttribute("x") || !vec2_elem.hasAttribute("y"))
            return {};

        x_ok = false;
        y_ok = false;

        vertex.m_TextureCoordinate.x() = vec2_elem.attribute("x").toFloat(&x_ok);
        vertex.m_TextureCoordinate.y() = vec2_elem.attribute("y").toFloat(&y_ok);

        if(!x_ok || !y_ok)
            return {};

        vertex_count++;
        vertex_elem = vertex_elem.nextSiblingElement();
    }

    return result;
}

ArtObjectParser::IndicesResult ArtObjectParser::parseIndices(const QDomElement& elem)
{
    if(elem.isNull())
        return {};

    IndicesResult::value_type result;

    // count indices
    auto index_elem = elem.firstChildElement("Index");
    auto index_count = size_t(0);

    while(!index_elem.isNull())
    {
        index_count++;

        index_elem = index_elem.nextSiblingElement();
    }

    result.resize(index_count);

    if(index_count % 3 != 0)
        return {};

    // set indices
    index_elem = elem.firstChildElement("Index");
    index_count = size_t(0);

    while(!index_elem.isNull())
    {
        auto index_ok = false;
        result[index_count] = index_elem.text().toInt(&index_ok);

        if(!index_ok)
            return {};

        index_count++;
        index_elem = index_elem.nextSiblingElement();
    }

    return result;
}

void ArtObjectParser::writeObj(const Geometry& geometry)
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
        ai_face.mIndices[1] = geometry.m_Indices[3 * i + 1];
        ai_face.mIndices[2] = geometry.m_Indices[3 * i + 2];
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

    success = exporter.Export(scene, "obj", m_Path.toStdString() + "/" + m_Name.toStdString() + ".obj");

    if(success != aiReturn_SUCCESS)
    {
        std::string export_error_string(exporter.GetErrorString());

        return;
    }

    QFile texture(m_Path + "/../" + m_Name + ".png");
    texture.copy(m_Path + "/" + m_Name + ".png");

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