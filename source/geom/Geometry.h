#pragma once

#include "geom/Vertex.h"

#include <QtCore/QFile>

#include <vector>

struct Geometry
{
	using Vertices = std::vector<Vertex>;
	using Indices = std::vector<size_t>;

	QString m_Name;
    QString m_TextureName;
    QString m_TextureOrgFile;
    QString m_OutPath;
    Vertices m_Vertices;
    Indices m_Indices;
};