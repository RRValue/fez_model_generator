#pragma once

#include "model/Texture.h"

#include <QtCore/QString>

#include <QtXml/QDomDocument>

namespace fez
{

class TextureParser
{
    using TextureResult = std::optional<Texture>;

public:
    TextureParser();
    ~TextureParser();

    TextureResult parse(const QString& path, const QString& name, const bool& isAnimated) noexcept;

private:
    QDomDocument m_Document;
};

} // namespace fez
