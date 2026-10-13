#pragma once

#include "math/Vector.h"
#include "model/Geometry.h"

namespace fez
{

// Texture-coordinate rectangle in FEZ convention (origin top-left, V pointing down): the whole texture is
// offset (0, 0), size (1, 1); an animated texture uses the offset and size of its first frame.
struct UvRect
{
    Vec2f m_Offset = Vec2f::Zero();
    Vec2f m_Size = Vec2f::Ones();
};

// Builds a plane quad of the given size centred at the origin in the XY plane, facing +Z, moved out along +Z by
// depthOffset (background planes 0.05, NPCs 0). Corners: 0 top-left, 1 bottom-left, 2 top-right, 3 bottom-right;
// indices 0,1,2, 2,1,3. V is flipped (v = 1 - y). Only vertices, indices and m_IsPlane are set; name, texture,
// opacity and sidedness are left to the caller.
Geometry makeTexturedQuad(Vec2f size, UvRect uv, float depthOffset);

} // namespace fez
