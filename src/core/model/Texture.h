#pragma once

#include "math/Vector.h"

#include <QtCore/QString>

#include <tuple>
#include <vector>

struct Texture
{
    using TextureAnimationOffsets = std::vector<std::tuple<unsigned int, Vec2f, Vec2f>>;

    QString m_TextureName = {};
    QString m_TextureOrgFile = {};
    bool m_IsAnimated = false;
    unsigned int m_Width = 0;
    unsigned int m_Height = 0;
    TextureAnimationOffsets m_TextureAnimationOffsets = {};
};