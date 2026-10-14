#include "geometry/FaceDecoding.h"

namespace fez
{

namespace
{

// Axis unit vectors, named by the direction they point. (The original parser called these pos_x ... neg_z with the
// sign inverted: its pos_x was (-1, 0, 0).)
const Vec3f negX{-1, 0, 0};
const Vec3f negY{0, -1, 0};
const Vec3f negZ{0, 0, -1};
const Vec3f posX{1, 0, 0};
const Vec3f posY{0, 1, 0};
const Vec3f posZ{0, 0, 1};

} // namespace

std::optional<Vec3f> decodeFaceNormal(int side)
{
    switch(side)
    {
        case 0: return negX;
        case 1: return negY;
        case 2: return negZ;
        case 3: return posX;
        case 4: return posY;
        case 5: return posZ;
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
