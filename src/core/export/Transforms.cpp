#include "export/Transforms.h"

#include <assimp/types.h>

#include <numbers>

namespace fez
{

Vec3f withLevelObjectOffset(const Vec3f& position)
{
    return Vec3f(position.x() + levelObjectOffset, position.y() + levelObjectOffset, position.z() + levelObjectOffset);
}

QuaternionF trileOrientation(int orientation)
{
    const aiQuaternion rotation = [](int value) {
        switch(value)
        {
            case 0: return aiQuaternion(std::numbers::pi_v<float>, 0.0f, 0.0f);
            case 1: return aiQuaternion(std::numbers::pi_v<float> * -0.5f, 0.0f, 0.0f);
            case 2: return aiQuaternion(0.0f, 0.0f, 0.0f);
            case 3: return aiQuaternion(std::numbers::pi_v<float> * 0.5f, 0.0f, 0.0f);
            default: return aiQuaternion();
        }
    }(orientation);

    return QuaternionF(rotation.w, rotation.x, rotation.y, rotation.z);
}

} // namespace fez
