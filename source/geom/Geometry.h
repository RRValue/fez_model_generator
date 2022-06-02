#pragma once

#include "geom/Vertex.h"

#include <vector>

struct Geometry
{
	using Vertices = std::vector<Vertex>;
	using Triangles = std::vector<size_t>;

	Vertices m_Vertices;
    Triangles m_Triangles;
};