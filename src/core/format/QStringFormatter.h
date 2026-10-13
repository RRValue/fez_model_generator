#pragma once

#include <QtCore/QByteArray>
#include <QtCore/QString>

#include <format>
#include <string_view>

// std::format support for QString: formats the string as UTF-8 and accepts the format specs of std::string_view
// (width, fill, alignment, precision), e.g. std::format("{:>10}", name).
template<>
struct std::formatter<QString, char> : std::formatter<std::string_view, char>
{
    template<typename FormatContext>
    auto format(const QString& value, FormatContext& context) const
    {
        const QByteArray utf8 = value.toUtf8();
        return std::formatter<std::string_view, char>::format(std::string_view(utf8.constData(), utf8.size()), context);
    }
};
