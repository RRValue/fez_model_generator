#pragma once

#include "math/Vector.h"

namespace fez
{

struct TrileEmplacement
{
    int m_Id = 0;
    Vec3f m_Emplacement = Vec3f::Zero();
    Vec3f m_Position = Vec3f::Zero();
    int m_Orientation = 0; // quarter turns about Y, 0 to 3
};

} // namespace fez
