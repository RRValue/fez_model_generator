#pragma once

#include "model/Vertex.h"

#include <QtCore/QString>

#include <vector>

struct Geometry
{
	using Vertices = std::vector<Vertex>;
	using Indices = std::vector<size_t>;

	QString m_Name;
    QString m_TextureName;
    QString m_TextureOrgFile;
    Vertices m_Vertices;
    Indices m_Indices;
};