#pragma once

#include "math/Vector.h"

#include <optional>

namespace fez
{

// Decodes the side index stored in a FEZ vertex <Normal> element (0-5) into the vertex normal:
// 0 -> (-1, 0, 0), 1 -> (0, -1, 0), 2 -> (0, 0, -1), 3 -> (1, 0, 0), 4 -> (0, 1, 0), 5 -> (0, 0, 1).
// Any other index gives std::nullopt.
std::optional<Vec3f> decodeFaceNormal(int side);

// Horizontal texture-coordinate offset of a face: the trile set atlas holds the six faces side by side, so U is shifted
// by the side index (0-5). Any other index gives 0.
float faceUvOffset(int side);

} // namespace fez
