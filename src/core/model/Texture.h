#pragma once

#include "math/Vector.h"

#include <QtCore/QString>

#include <vector>

namespace fez
{

struct AnimationFrame
{
    unsigned int m_Duration = 0;
    Vec2f m_Offset = Vec2f::Zero();
    Vec2f m_Size = Vec2f::Zero();
};

struct Texture
{
    using TextureAnimationOffsets = std::vector<AnimationFrame>;

    QString m_TextureName = {};
    QString m_TextureOrgFile = {};
    bool m_IsAnimated = false;
    unsigned int m_Width = 0;
    unsigned int m_Height = 0;
    TextureAnimationOffsets m_TextureAnimationOffsets = {};
};

} // namespace fez
