#pragma once

#include "math/Quaternion.h"
#include "math/Vector.h"

#include <QtCore/QString>

struct BackgroundPlane
{
    QString m_Name;
    Vec3f m_Position = Vec3f::Zero();
    QuaternionF m_Rotation = QuaternionF::Identity();
    Vec3f m_Scale = Vec3f::Zero();
    Geometry m_Geometry = {};
};