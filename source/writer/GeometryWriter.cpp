#include "writer/GeometryWriter.h"

#include <QtCore/QFile>
#include <QtCore/QDir>

#include <assimp/Exporter.hpp>
#include <assimp/scene.h>

void GeometryWriter::writeObj(const Geometry& geometry)
{
    m_SaveName = geometry.m_Name;
    addGeometry(geometry);
    save();
}
