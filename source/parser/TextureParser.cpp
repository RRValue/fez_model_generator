#include "parser/TextureParser.h"

#include <QtCore/QDir>
#include <QtCore/QFile>
#include <QtCore/QFileInfo>

#include <QtGui/QImage>

TextureParser::TextureParser() : m_Document{}
{
}

TextureParser::~TextureParser()
{
}

TextureParser::TextureResult TextureParser::parse(const QString& path, const bool& isAnimated) noexcept
{
    qDebug() << "parsing: " << path;

    Texture result;

    result.m_TextureOrgFile = path + (isAnimated ? ".ani.png" : ".png");

    if(!QFile(result.m_TextureOrgFile).exists())
        return {};

    result.m_IsAnimated = isAnimated;
    result.m_TextureName = QFileInfo(result.m_TextureOrgFile).fileName();

    if(!isAnimated)
    {
        const auto background_plane_image = QImage(result.m_TextureOrgFile);

        if(background_plane_image.isNull())
            return {};

        result.m_Width = background_plane_image.width();
        result.m_Height = background_plane_image.height();

        return result;
    }

    const auto xml_file_path = path + ".xml";

    QFileInfo info(xml_file_path);

    if(!info.exists())
        return {};

    m_Document.clear();

    // load
    auto xml_file = QFile(xml_file_path);

    if(!xml_file.exists())
        return {};

    if(!xml_file.open(QIODevice::OpenModeFlag::ReadOnly))
        return {};

    QString error_msg;
    int error_line;
    int error_column;

    if(!m_Document.setContent(&xml_file, &error_msg, &error_line, &error_column))
    {
        qDebug() << "Error: \"" + error_msg + "\" at line: " + QString::number(error_line) + " Columns: " + QString::number(error_column);

        return {};
    }

    // parse
    const auto animated_texture_pc_elem = m_Document.firstChildElement("AnimatedTexturePC");

    if(animated_texture_pc_elem.isNull())
        return {};

    if(!animated_texture_pc_elem.hasAttribute("actualWidth") || !animated_texture_pc_elem.hasAttribute("actualHeight"))
        return {};

    auto actual_width_ok = false;
    auto actual_height_ok = false;
    auto width_ok = false;
    auto height_ok = false;

    result.m_Width = animated_texture_pc_elem.attribute("actualWidth").toUInt(&actual_width_ok);
    result.m_Height = animated_texture_pc_elem.attribute("actualHeight").toUInt(&actual_height_ok);

    const auto texture_width = animated_texture_pc_elem.attribute("width").toUInt(&width_ok);
    const auto texture_height = animated_texture_pc_elem.attribute("height").toUInt(&height_ok);

    if(!actual_width_ok || !actual_height_ok)
        return {};

    // parse frames
    const auto frames_elem = animated_texture_pc_elem.firstChildElement("Frames");

    if(frames_elem.isNull())
        return {};

    // read frames
    auto frame_pc_elem = frames_elem.firstChildElement("FramePC");
    auto frame_pc_count = size_t(0);

    while(!frame_pc_elem.isNull())
    {
        frame_pc_count++;
        frame_pc_elem = frame_pc_elem.nextSiblingElement();
    }

    if(frame_pc_count == 0)
        return {};

    frame_pc_elem = frames_elem.firstChildElement("FramePC");
    result.m_TextureAnimationOffsets = Texture::TextureAnimationOffsets(frame_pc_count);
    frame_pc_count = 0;

    while(!frame_pc_elem.isNull())
    {
        if(!frame_pc_elem.hasAttribute("duration"))
            return {};

        auto duration_ok = false;
        const auto duration = frame_pc_elem.attribute("duration").toUInt(&duration_ok);

        if(!duration_ok)
            return {};

        const auto rectangle_elem = frame_pc_elem.firstChildElement("Rectangle");

        if(rectangle_elem.isNull())
            return {};

        if(!rectangle_elem.hasAttribute("x") || !rectangle_elem.hasAttribute("y") || !rectangle_elem.hasAttribute("w") || !rectangle_elem.hasAttribute("h"))
            return {};

        auto x_ok = false;
        auto y_ok = false;
        auto w_ok = false;
        auto h_ok = false;

        const auto pos = Vec2f{rectangle_elem.attribute("x").toFloat(&x_ok) / float(texture_width),  //
                                  rectangle_elem.attribute("y").toFloat(&y_ok) / float(texture_height)};
        const auto size = Vec2f{rectangle_elem.attribute("w").toFloat(&w_ok) / float(texture_width),  //
                                  rectangle_elem.attribute("h").toFloat(&h_ok) / float(texture_height)};

        if(!x_ok || !y_ok || !w_ok || !h_ok)
            return {};

        result.m_TextureAnimationOffsets[frame_pc_count++] = {duration, pos, size};

        frame_pc_elem = frame_pc_elem.nextSiblingElement();
    }

    return result;
}