#include "geometry/TexturedQuad.h"

namespace fez
{

Geometry makeTexturedQuad(Vec2f size, UvRect uv, float depthOffset)
{
    const float w = size.x();
    const float h = size.y();

    const Vec3f normal = Vec3f{0.0f, 0.0f, 1.0f};

    Vec3f p0 = Vec3f{-w / 2, h / 2, 0.0f};
    Vec3f p1 = Vec3f{-w / 2, -h / 2, 0.0f};
    Vec3f p2 = Vec3f{w / 2, h / 2, 0.0f};
    Vec3f p3 = Vec3f{w / 2, -h / 2, 0.0f};

    // The offset vector is only added when there is an offset: adding (0, 0, 0) would turn a -0 coordinate into +0,
    // and the NPC quads never added it.
    if(depthOffset != 0.0f)
    {
        const Vec3f move_out_vec = normal * depthOffset;

        p0 += move_out_vec;
        p1 += move_out_vec;
        p2 += move_out_vec;
        p3 += move_out_vec;
    }

    const Vec2f texture_coord_p = uv.m_Offset;
    const Vec2f texture_coord_s = texture_coord_p + uv.m_Size;

    Geometry result;

    result.m_IsPlane = true;
    result.m_Vertices = Geometry::Vertices(4);
    result.m_Indices = Geometry::Indices{0, 1, 2, 2, 1, 3};

    result.m_Vertices[0] = {p0, normal, {texture_coord_p.x(), 1.0f - texture_coord_p.y()}};
    result.m_Vertices[1] = {p1, normal, {texture_coord_p.x(), 1.0f - texture_coord_s.y()}};
    result.m_Vertices[2] = {p2, normal, {texture_coord_s.x(), 1.0f - texture_coord_p.y()}};
    result.m_Vertices[3] = {p3, normal, {texture_coord_s.x(), 1.0f - texture_coord_s.y()}};

    return result;
}

} // namespace fez
