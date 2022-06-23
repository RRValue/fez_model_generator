#pragma once

#include "model/Vertex.h"
#include "model/Texture.h"

#include <QtCore/QString>

#include <vector>

struct Geometry
{
	using Vertices = std::vector<Vertex>;
	using Indices = std::vector<size_t>;

	QString m_Name = {};
    Vertices m_Vertices = {};
    Indices m_Indices = {};
    Texture m_Texture = {};
    float m_Opacity = 1.0f;
    bool m_DoubleSided = true;
    bool m_IsBackgroundPlane = false;
};