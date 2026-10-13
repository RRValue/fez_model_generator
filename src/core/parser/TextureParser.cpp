#include "parser/TextureParser.h"

#include "xml/Xml.h"

#include <QtCore/QFile>

#include <QtGui/QImage>

#include <qdebug.h>

#include <utility>

namespace fez
{

namespace
{

// One FramePC: duration and the Rectangle divided by the sheet size.
std::expected<AnimationFrame, Error> parseFrame(const QDomElement& frame_elem, unsigned int sheetWidth, unsigned int sheetHeight)
{
    const auto duration = attribute<unsigned int>(frame_elem, "duration");

    if(!duration)
        return std::unexpected(duration.error());

    const auto rectangle_elem = childElement(frame_elem, "Rectangle");

    if(!rectangle_elem)
        return std::unexpected(rectangle_elem.error());

    const auto x = attribute<float>(*rectangle_elem, "x");

    if(!x)
        return std::unexpected(x.error());

    const auto y = attribute<float>(*rectangle_elem, "y");

    if(!y)
        return std::unexpected(y.error());

    const auto w = attribute<float>(*rectangle_elem, "w");

    if(!w)
        return std::unexpected(w.error());

    const auto h = attribute<float>(*rectangle_elem, "h");

    if(!h)
        return std::unexpected(h.error());

    const auto pos = Vec2f{*x / float(sheetWidth), *y / float(sheetHeight)};
    const auto size = Vec2f{*w / float(sheetWidth), *h / float(sheetHeight)};

    return AnimationFrame{*duration, pos, size};
}

} // namespace

std::expected<AnimatedTexture, Error> parseAnimatedTexture(const QString& path)
{
    const auto document = loadXml(path);

    if(!document)
    {
        if(document.error().kind == ErrorKind::XmlSyntax)
            qDebug() << "Error: \"" + QString::fromStdString(document.error().message) + "\" at " + QString::fromStdString(document.error().source);

        return std::unexpected(document.error());
    }

    // QDomDocument::firstChildElement: the document element, if it is named AnimatedTexturePC
    const auto animated_texture_pc_elem = document->firstChildElement("AnimatedTexturePC");

    if(animated_texture_pc_elem.isNull())
        return std::unexpected(Error{ErrorKind::MissingElement, "missing element 'AnimatedTexturePC'", "AnimatedTexturePC"});

    const auto actual_width = attribute<unsigned int>(animated_texture_pc_elem, "actualWidth");

    if(!actual_width)
        return std::unexpected(actual_width.error());

    const auto actual_height = attribute<unsigned int>(animated_texture_pc_elem, "actualHeight");

    if(!actual_height)
        return std::unexpected(actual_height.error());

    const auto texture_width = attribute<unsigned int>(animated_texture_pc_elem, "width");

    if(!texture_width)
        return std::unexpected(texture_width.error());

    const auto texture_height = attribute<unsigned int>(animated_texture_pc_elem, "height");

    if(!texture_height)
        return std::unexpected(texture_height.error());

    const auto frames_elem = childElement(animated_texture_pc_elem, "Frames");

    if(!frames_elem)
        return std::unexpected(frames_elem.error());

    // From the first FramePC on, every following sibling element is a frame (no name filter, as before).
    const auto first_frame = childElement(*frames_elem, "FramePC");

    if(!first_frame)
        return std::unexpected(first_frame.error());

    AnimatedTexture result;
    result.m_Width = *actual_width;
    result.m_Height = *actual_height;

    for(auto iter = ChildElementRange::Iterator(*first_frame, QString()); iter != ChildElementRange::Iterator(); ++iter)
    {
        auto frame = parseFrame(*iter, *texture_width, *texture_height);

        if(!frame)
            return std::unexpected(frame.error());

        result.m_Frames.push_back(std::move(*frame));
    }

    return result;
}

std::expected<Texture, Error> loadTexture(const QString& dir, const QString& name, bool animated)
{
    qDebug() << "parsing: " << dir << "/" << name;

    Texture result;

    result.m_TextureOrgFile = dir + "/" + name + (animated ? ".ani.png" : ".png");

    if(!QFile(result.m_TextureOrgFile).exists())
        return std::unexpected(Error{ErrorKind::FileNotFound, "file not found", result.m_TextureOrgFile.toStdString()});

    result.m_IsAnimated = animated;
    result.m_TextureName = name + (animated ? ".ani.png" : ".png");

    if(!animated)
    {
        const auto image = QImage(result.m_TextureOrgFile);

        if(image.isNull())
            return std::unexpected(Error{ErrorKind::Io, "cannot read image", result.m_TextureOrgFile.toStdString()});

        result.m_Width = image.width();
        result.m_Height = image.height();

        return result;
    }

    auto animated_texture = parseAnimatedTexture(dir + "/" + name + ".xml");

    if(!animated_texture)
        return std::unexpected(animated_texture.error());

    result.m_Width = animated_texture->m_Width;
    result.m_Height = animated_texture->m_Height;
    result.m_TextureAnimationOffsets = std::move(animated_texture->m_Frames);

    return result;
}

} // namespace fez
