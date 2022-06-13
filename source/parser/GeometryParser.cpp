#include "parser/GeometryParser.h"

#include "math/Vector.h"

#include <QtCore/QDir>
#include <QtCore/QFile>
#include <QtCore/QFileInfo>

#include <assimp/Exporter.hpp>
#include <assimp/scene.h>

#include <qdebug.h>

GeometryParser::GeometryParser()
{
}

GeometryParser::~GeometryParser()
{
}

bool GeometryParser::parseGeometry(const QDomElement& elem, aiMesh* mesh, aiMaterial* material, const QString& textureName)
{
    if(elem.isNull() || elem.nodeName() != "ShaderInstancedIndexedPrimitives")
        return false;

    if(!parseVertices(elem.firstChildElement("Vertices"), mesh))
        return false;

    if(!parseIndices(elem.firstChildElement("Indices"), mesh))
        return false;

    // load material
    aiString fileName(textureName.toStdString() + ".png");
    material->AddProperty(&fileName, AI_MATKEY_NAME);
    material->AddProperty(&fileName, AI_MATKEY_TEXTURE(aiTextureType_DIFFUSE, 0));

    return true;
}

bool GeometryParser::parseVertices(const QDomElement& elem, aiMesh* mesh)
{
    // count vertices
    auto vertex_elem = elem.firstChildElement("VertexPositionNormalTextureInstance");
    auto vertex_count = size_t(0);

    while(!vertex_elem.isNull())
    {
        vertex_count++;

        vertex_elem = vertex_elem.nextSiblingElement();
    }

    if(vertex_count == 0)
        return false;

    mesh->mNumVertices = (unsigned int)vertex_count;
    mesh->mVertices = new aiVector3D[vertex_count];
    mesh->mNormals = new aiVector3D[vertex_count];
    mesh->mTextureCoords[0] = new aiVector3D[vertex_count];

    // set vertices
    vertex_elem = elem.firstChildElement("VertexPositionNormalTextureInstance");
    vertex_count = size_t(0);

    while(!vertex_elem.isNull())
    {
        const auto position_elem = vertex_elem.firstChildElement("Position");
        const auto normal_elem = vertex_elem.firstChildElement("Normal");
        const auto texture_coord_elem = vertex_elem.firstChildElement("TextureCoord");

        if(position_elem.isNull() || normal_elem.isNull() || texture_coord_elem.isNull())
            return false;

        // position
        const auto vec3_elem = position_elem.firstChildElement("Vector3");

        if(vec3_elem.isNull())
            return false;

        if(!vec3_elem.hasAttribute("x") || !vec3_elem.hasAttribute("y") || !vec3_elem.hasAttribute("z"))
            return false;

        auto x_ok = false;
        auto y_ok = false;
        auto z_ok = false;

        mesh->mVertices[vertex_count] = aiVector3D{vec3_elem.attribute("x").toFloat(&x_ok),  //
                                                   vec3_elem.attribute("y").toFloat(&y_ok),  //
                                                   vec3_elem.attribute("z").toFloat(&z_ok)};

        if(!x_ok || !y_ok || !z_ok)
            return false;

        // normal
        auto side_index_ok = false;
        const auto side_index = normal_elem.text().toInt(&side_index_ok);

        if(!side_index_ok)
            return false;

        static const auto pos_x = Vec3f{1, 0, 0};
        static const auto pos_y = Vec3f{0, 1, 0};
        static const auto pos_z = Vec3f{0, 0, 1};
        static const auto neg_x = Vec3f{-1, 0, 0};
        static const auto neg_y = Vec3f{0, -1, 0};
        static const auto neg_z = Vec3f{0, 0, -1};

        Vec3f normal;

        switch(side_index)
        {
            case 0: normal = pos_x; break;
            case 1: normal = pos_y; break;
            case 2: normal = pos_z; break;
            case 3: normal = neg_x; break;
            case 4: normal = neg_y; break;
            case 5: normal = neg_z; break;
            default: return false;
        }

        mesh->mNormals[vertex_count] = aiVector3D{normal.x(),  //
                                                  normal.y(),  //
                                                  normal.z()};

        // texture coordinate
        const auto vec2_elem = texture_coord_elem.firstChildElement("Vector2");

        if(vec2_elem.isNull())
            return false;

        if(!vec2_elem.hasAttribute("x") || !vec2_elem.hasAttribute("y"))
            return false;

        x_ok = false;
        y_ok = false;

        Vec2f tex_coord = Vec2f::Zero();

        tex_coord.x() = vec2_elem.attribute("x").toFloat(&x_ok);
        tex_coord.y() = 1.0f - vec2_elem.attribute("y").toFloat(&y_ok);

        switch(side_index)
        {
            case 0: break;
            case 1: tex_coord.x() += 1.0; break;
            case 2: tex_coord.x() += 2.0; break;
            case 3: tex_coord.x() += 3.0; break;
            case 4: tex_coord.x() += 4.0; break;
            case 5: tex_coord.x() += 5.0; break;
            default: break;
        }

        mesh->mTextureCoords[0][vertex_count] = aiVector3D{tex_coord.x(),  //
                                                           tex_coord.y(),  //
                                                           0};

        if(!x_ok || !y_ok)
            return false;

        vertex_count++;
        vertex_elem = vertex_elem.nextSiblingElement();
    }

    return true;
}

bool GeometryParser::parseIndices(const QDomElement& elem, aiMesh* mesh)
{
    if(elem.isNull())
        return false;

    // count indices
    auto index_elem = elem.firstChildElement("Index");
    auto index_count = size_t(0);

    while(!index_elem.isNull())
    {
        index_count++;

        index_elem = index_elem.nextSiblingElement();
    }

    if(index_count % 3 != 0)
        return false;

    mesh->mNumFaces = (unsigned int)(index_count / 3);
    mesh->mFaces = new aiFace[mesh->mNumFaces];
    
    // set indices
    index_elem = elem.firstChildElement("Index");
    index_count = size_t(0);
    
    while(!index_elem.isNull())
    {
        auto index_ok = false;
        const auto index = index_elem.text().toInt(&index_ok);

        if(!index_ok)
            return false;

        auto& face = mesh->mFaces[index_count / 3];

        if(index_count % 3 == 0)
        {
            face.mNumIndices = 3;
            face.mIndices = new unsigned int[3];
        }

        face.mIndices[index_count % 3] = index;

        index_count++;
        index_elem = index_elem.nextSiblingElement();
    }

    return true;
}