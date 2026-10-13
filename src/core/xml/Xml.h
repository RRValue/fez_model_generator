#pragma once

#include "error/Error.h"
#include "math/Quaternion.h"
#include "math/Vector.h"

#include <QtCore/QByteArray>
#include <QtCore/QString>
#include <QtXml/QDomDocument>
#include <QtXml/QDomElement>

#include <concepts>
#include <cstddef>
#include <expected>
#include <iterator>
#include <string>
#include <utility>

namespace fez
{

// --- Documents -----------------------------------------------------------------------------------------------------

// Reads and parses an XML file.
// Errors: FileNotFound (no such path), Io (not a readable file), XmlSyntax (source "<path>:<line>:<column>").
std::expected<QDomDocument, Error> loadXml(const QString& path);

// Parses XML from memory; sourceName stands in for the path in XmlSyntax errors.
std::expected<QDomDocument, Error> parseXml(const QByteArray& content, const QString& sourceName);

// --- Element paths (error sources) ---------------------------------------------------------------------------------

// Slash-separated tag names from the document element down to element, e.g.
// "Level/ArtObjects/Entry[3]/ArtObjectInstance". An element with same-named siblings gets its 1-based position among
// them in brackets (as in XPath). A null element gives "".
std::string elementPath(const QDomElement& element);

// --- Child elements ------------------------------------------------------------------------------------------------

// First direct child element named name.
// Errors: MissingElement (source "<parent path>/<name>").
std::expected<QDomElement, Error> childElement(const QDomElement& parent, const QString& name);

// Forward range over the direct child elements of a parent with a given tag name (all child elements if the name is
// empty), in document order. Empty for a null parent.
class ChildElementRange
{
public:
    class Iterator
    {
    public:
        using value_type = QDomElement;
        using difference_type = std::ptrdiff_t;
        using reference = const QDomElement&;
        using pointer = const QDomElement*;
        using iterator_category = std::forward_iterator_tag;

        Iterator() = default;
        Iterator(QDomElement element, QString name)
            : m_Element(std::move(element))
            , m_Name(std::move(name))
        {
        }

        reference operator*() const { return m_Element; }
        pointer operator->() const { return &m_Element; }

        Iterator& operator++()
        {
            m_Element = m_Element.nextSiblingElement(m_Name);
            return *this;
        }

        Iterator operator++(int)
        {
            auto previous = *this;
            ++*this;
            return previous;
        }

        // Equal when both refer to the same node; every past-the-end iterator holds a null element.
        friend bool operator==(const Iterator& lhs, const Iterator& rhs) { return lhs.m_Element == rhs.m_Element; }

    private:
        QDomElement m_Element;
        QString m_Name;
    };

    ChildElementRange(const QDomElement& parent, QString name)
        : m_First(parent.firstChildElement(name))
        , m_Name(std::move(name))
    {
    }

    Iterator begin() const { return Iterator(m_First, m_Name); }
    Iterator end() const { return Iterator(); }

private:
    QDomElement m_First;
    QString m_Name;
};

static_assert(std::forward_iterator<ChildElementRange::Iterator>);

inline ChildElementRange childElements(const QDomElement& parent, const QString& name = {})
{
    return ChildElementRange(parent, name);
}

// --- Attributes ----------------------------------------------------------------------------------------------------

template<typename T>
concept XmlAttributeType =
    std::same_as<T, QString> || std::same_as<T, float> || std::same_as<T, int> || std::same_as<T, unsigned int>;

// Value of attribute name of element, converted with QString::toFloat / toInt / toUInt (a QString is returned as is).
// Errors: MissingAttribute, BadValue (not a valid number); source "<element path>@<name>".
template<XmlAttributeType T>
std::expected<T, Error> attribute(const QDomElement& element, const QString& name);

template<>
std::expected<QString, Error> attribute<QString>(const QDomElement& element, const QString& name);
template<>
std::expected<float, Error> attribute<float>(const QDomElement& element, const QString& name);
template<>
std::expected<int, Error> attribute<int>(const QDomElement& element, const QString& name);
template<>
std::expected<unsigned int, Error> attribute<unsigned int>(const QDomElement& element, const QString& name);

// "true" / "false", case-insensitive.
// Errors: MissingAttribute, BadValue (any other text); source "<element path>@<name>".
std::expected<bool, Error> readBool(const QDomElement& element, const QString& name);

// Float attributes x, y of a Vector2 element.
std::expected<Vec2f, Error> readVector2(const QDomElement& element);

// Float attributes x, y, z of a Vector3 element.
std::expected<Vec3f, Error> readVector3(const QDomElement& element);

// Float attributes w, x, y, z of a Quaternion element, constructed as QuaternionF{w, x, y, z} (as the parsers do).
std::expected<QuaternionF, Error> readQuaternion(const QDomElement& element);

} // namespace fez
