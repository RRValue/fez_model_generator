#pragma once

#include "math/Quaternion.h"
#include "math/Vector.h"

namespace fez
{

// Placement of one node in a scene: the node matrix is aiMatrix4x4(scale, rotation, position).
struct Transform
{
    Vec3f m_Position = Vec3f::Zero();
    QuaternionF m_Rotation = QuaternionF::Identity();
    Vec3f m_Scale = Vec3f::Ones();
};

// Art objects and background planes are placed relative to trile centres: their level position is shifted by this
// offset on every axis (triles and NPCs are not shifted).
inline constexpr float levelObjectOffset = -0.5f;

// position + levelObjectOffset per component (bit-identical to position - 0.5f).
Vec3f withLevelObjectOffset(const Vec3f& position);

// Rotation of a trile emplacement about Y: orientation 0 -> pi, 1 -> -pi/2, 2 -> 0, 3 -> pi/2, anything else -> identity.
// The values are those of assimp's Euler-angle quaternion constructor (bit-exact), as the old LevelWriter used.
QuaternionF trileOrientation(int orientation);

} // namespace fez
