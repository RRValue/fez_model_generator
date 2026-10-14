#include "writer/Writer.h"

#include <QtCore/QFile>
#include <QtCore/QDir>

#include <assimp/Exporter.hpp>
#include <assimp/scene.h>

namespace fez
{

Writer::Writer(const QString& path) : m_Path{path}
{
    // make path
    // Writers run in parallel and share missing parent directories (e.g. <output>/lv_export): QDir::mkpath fails for
    // a thread that creates such a parent at the same moment as another one, so it is retried (the parent then exists).
    QDir dir(m_Path);

    for(int attempt = 0; attempt < 3 && !dir.exists(); ++attempt)
        dir.mkpath(".");
}

Writer::MeshId Writer::addGeometry(const Geometry& geometry)
{
    const auto mesh_id = m_Builder.addMesh(geometry);

    if(!mesh_id)
        return {};

    m_Textures.push_back(std::make_pair(geometry.m_Texture.m_TextureOrgFile, m_Path + "/" + geometry.m_Texture.m_TextureName));

    return mesh_id;
}

void Writer::save()
{
    // save
    const auto scene = m_Builder.build();

    Assimp::Exporter exporter;
    aiReturn success = exporter.Export(scene.get(), "obj", m_Path.toStdString() + "/" + m_SaveName.toStdString() + ".obj");
    
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

} // namespace fez
