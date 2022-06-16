#pragma once

#include "model/Geometry.h"

struct TrileEmplacement
{
    int m_Id = 0;
    Vec3f m_Emplacement = Vec3f::Zero();
    Vec3f m_Position = Vec3f::Zero();
    unsigned int m_Orintation = 0;
    Geometry m_Geometry = {};
};