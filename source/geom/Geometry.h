#pragma once

#include "geom/Vertex.h"

#include <vector>

struct Geometry
{
	using Vertices = std::vector<Vertex>;
	using Indices = std::vector<size_t>;

	Vertices m_Vertices;
    Indices m_Indices;
};