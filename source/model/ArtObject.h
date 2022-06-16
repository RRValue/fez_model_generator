#pragma once

#include "model/Geometry.h"

#include "math/Quaternion.h"

struct ArtObject
{
    QString m_Name;
    Vec3f m_Position = Vec3f::Zero();
    QuaternionF m_Rotation = QuaternionF::Identity();
    Vec3f m_Scale = Vec3f::Zero();
    Geometry m_Geometry = {};
};