#pragma once

#include "math/Quaternion.h"
#include "math/Vector.h"

#include <QtCore/QString>

struct Character
{
    QString m_Name;
    Vec3f m_Position = Vec3f::Zero();
    Geometry m_Geometry = {};
};