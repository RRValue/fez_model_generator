#pragma once

#include "math/Vector.h"

namespace fez
{

struct Vertex
{
    Vec3f m_Position = Vec3f::Zero();
    Vec3f m_Normal = Vec3f::UnitY();
    Vec2f m_TextureCoordinate = Vec2f::Zero();
};

} // namespace fez
