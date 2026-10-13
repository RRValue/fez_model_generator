#include "xml/Xml.h"

#include "format/QStringFormatter.h"

#include <QtCore/QFile>
#include <QtCore/QFileInfo>

#include <format>
#include <string_view>

namespace fez
{

namespace
{

std::string attributePath(const QDomElement& element, const QString& name)
{
    return std::format("{}@{}", elementPath(element), name);
}

Error missingAttribute(const QDomElement& element, const QString& name)
{
    return Error{ErrorKind::MissingAttribute, std::format("missing attribute '{}'", name), attributePath(element, name)};
}

Error badValue(const QDomElement& element, const QString& name, std::string_view expected, const QString& value)
{
    return Error{ErrorKind::BadValue,
                 std::format("attribute '{}' is not a valid {}: '{}'", name, expected, value),
                 attributePath(element, name)};
}

// Reads attribute name and converts it with convert(text, &ok).
template<typename T, typename Convert>
std::expected<T, Error> readNumber(const QDomElement& element, const QString& name, std::string_view typeName, Convert convert)
{
    if(!element.hasAttribute(name))
        return std::unexpected(missingAttribute(element, name));

    const auto text = element.attribute(name);
    auto ok = false;
    const T value = convert(text, &ok);

    if(!ok)
        return std::unexpected(badValue(element, name, typeName, text));

    return value;
}

} // namespace

std::expected<QDomDocument, Error> loadXml(const QString& path)
{
    const QFileInfo info(path);

    if(!info.exists())
        return std::unexpected(Error{ErrorKind::FileNotFound, "file not found", path.toStdString()});

    QFile file(path);

    if(!info.isFile() || !file.open(QIODevice::OpenModeFlag::ReadOnly))
        return std::unexpected(Error{ErrorKind::Io, std::format("cannot open file: {}", file.errorString()), path.toStdString()});

    return parseXml(file.readAll(), path);
}

std::expected<QDomDocument, Error> parseXml(const QByteArray& content, const QString& sourceName)
{
    QDomDocument document;
    const auto result = document.setContent(content);

    if(!result)
        return std::unexpected(Error{ErrorKind::XmlSyntax,
                                     std::format("{}", result.errorMessage),
                                     std::format("{}:{}:{}", sourceName, result.errorLine, result.errorColumn)});

    return document;
}

std::string elementPath(const QDomElement& element)
{
    std::string path;

    for(auto current = element; !current.isNull(); current = current.parentNode().toElement())
    {
        const auto name = current.tagName();
        auto segment = name.toStdString();

        if(!current.previousSiblingElement(name).isNull() || !current.nextSiblingElement(name).isNull())
        {
            auto position = 1;

            for(auto sibling = current.previousSiblingElement(name); !sibling.isNull(); sibling = sibling.previousSiblingElement(name))
                ++position;

            segment += std::format("[{}]", position);
        }

        path = path.empty() ? std::move(segment) : std::format("{}/{}", segment, path);
    }

    return path;
}

std::expected<QDomElement, Error> childElement(const QDomElement& parent, const QString& name)
{
    const auto child = parent.firstChildElement(name);

    if(child.isNull())
        return std::unexpected(Error{ErrorKind::MissingElement,
                                     std::format("missing element '{}'", name),
                                     std::format("{}/{}", elementPath(parent), name)});

    return child;
}

template<>
std::expected<QString, Error> attribute<QString>(const QDomElement& element, const QString& name)
{
    if(!element.hasAttribute(name))
        return std::unexpected(missingAttribute(element, name));

    return element.attribute(name);
}

template<>
std::expected<float, Error> attribute<float>(const QDomElement& element, const QString& name)
{
    return readNumber<float>(element, name, "float", [](const QString& text, bool* ok) { return text.toFloat(ok); });
}

template<>
std::expected<int, Error> attribute<int>(const QDomElement& element, const QString& name)
{
    return readNumber<int>(element, name, "int", [](const QString& text, bool* ok) { return text.toInt(ok); });
}

template<>
std::expected<unsigned int, Error> attribute<unsigned int>(const QDomElement& element, const QString& name)
{
    return readNumber<unsigned int>(element, name, "unsigned int", [](const QString& text, bool* ok) { return text.toUInt(ok); });
}

std::expected<bool, Error> readBool(const QDomElement& element, const QString& name)
{
    const auto text = attribute<QString>(element, name);

    if(!text)
        return std::unexpected(text.error());

    if(text->compare(QStringLiteral("true"), Qt::CaseInsensitive) == 0)
        return true;

    if(text->compare(QStringLiteral("false"), Qt::CaseInsensitive) == 0)
        return false;

    return std::unexpected(badValue(element, name, "bool (true/false)", *text));
}

std::expected<Vec2f, Error> readVector2(const QDomElement& element)
{
    const auto x = attribute<float>(element, QStringLiteral("x"));
    if(!x)
        return std::unexpected(x.error());

    const auto y = attribute<float>(element, QStringLiteral("y"));
    if(!y)
        return std::unexpected(y.error());

    return Vec2f(*x, *y);
}

std::expected<Vec3f, Error> readVector3(const QDomElement& element)
{
    const auto x = attribute<float>(element, QStringLiteral("x"));
    if(!x)
        return std::unexpected(x.error());

    const auto y = attribute<float>(element, QStringLiteral("y"));
    if(!y)
        return std::unexpected(y.error());

    const auto z = attribute<float>(element, QStringLiteral("z"));
    if(!z)
        return std::unexpected(z.error());

    return Vec3f(*x, *y, *z);
}

std::expected<QuaternionF, Error> readQuaternion(const QDomElement& element)
{
    const auto w = attribute<float>(element, QStringLiteral("w"));
    if(!w)
        return std::unexpected(w.error());

    const auto x = attribute<float>(element, QStringLiteral("x"));
    if(!x)
        return std::unexpected(x.error());

    const auto y = attribute<float>(element, QStringLiteral("y"));
    if(!y)
        return std::unexpected(y.error());

    const auto z = attribute<float>(element, QStringLiteral("z"));
    if(!z)
        return std::unexpected(z.error());

    return QuaternionF{*w, *x, *y, *z};
}

} // namespace fez
