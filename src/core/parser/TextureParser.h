#pragma once

#include "error/Error.h"
#include "model/Texture.h"

#include <QtCore/QString>

#include <expected>

namespace fez
{

// Content of an AnimatedTexturePC XML file.
//  - m_Width, m_Height: actualWidth / actualHeight (the size of one frame in pixels)
//  - m_Frames: one frame per FramePC with duration, and the Rectangle x, y / w, h divided by width / height (the size
//    of the whole sheet) as floats
struct AnimatedTexture
{
    unsigned int m_Width = 0;
    unsigned int m_Height = 0;
    Texture::TextureAnimationOffsets m_Frames = {};
};

// Reads an AnimatedTexturePC XML file (document element AnimatedTexturePC with unsigned attributes width, height,
// actualWidth, actualHeight and a Frames child of FramePC elements, each with an unsigned duration and a Rectangle
// child with float attributes x, y, w, h). Frame iteration starts at the first FramePC and then visits every following
// sibling element, whatever its name (as before). A zero width or height is not checked (the division gives inf/nan).
// Errors: FileNotFound, Io, XmlSyntax (loadXml); MissingElement (no AnimatedTexturePC document element, source
// "AnimatedTexturePC"; missing Frames, Rectangle; no FramePC at all, source ".../Frames/FramePC"); MissingAttribute,
// BadValue (attributes). Any invalid frame fails the whole texture.
std::expected<AnimatedTexture, Error> parseAnimatedTexture(const QString& path);

// Loads the texture <dir>/<name> (name may contain subdirectories, e.g. "<character>/<action>").
//  - static:   m_TextureName "<name>.png", m_TextureOrgFile "<dir>/<name>.png"; m_Width / m_Height are the size of
//              that PNG (it must be a readable image)
//  - animated: m_TextureName "<name>.ani.png", m_TextureOrgFile "<dir>/<name>.ani.png" (only checked for existence),
//              size and m_TextureAnimationOffsets from parseAnimatedTexture("<dir>/<name>.xml")
// Errors: FileNotFound (PNG or XML missing, source the file path); Io (static PNG not readable as an image); errors of
// parseAnimatedTexture.
std::expected<Texture, Error> loadTexture(const QString& dir, const QString& name, bool animated);

} // namespace fez
