#include "geometry/FaceDecoding.h"

namespace fez
{

std::optional<Vec3f> decodeFaceNormal(int side)
{
    switch(side)
    {
        case 0: return Vec3f{-1, 0, 0};
        case 1: return Vec3f{0, -1, 0};
        case 2: return Vec3f{0, 0, -1};
        case 3: return Vec3f{1, 0, 0};
        case 4: return Vec3f{0, 1, 0};
        case 5: return Vec3f{0, 0, 1};
        default: return std::nullopt;
    }
}

float faceUvOffset(int side)
{
    if(side < 0 || side > 5)
        return 0.0f;

    return static_cast<float>(side);
}

} // namespace fez
