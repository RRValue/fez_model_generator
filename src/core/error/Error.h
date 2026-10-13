#pragma once

#include <string>
#include <string_view>

namespace fez
{

// What went wrong; decides how the pipeline reports a failed asset.
enum class ErrorKind
{
    FileNotFound,      // a file that should be read does not exist
    XmlSyntax,         // the XML is not well-formed
    MissingElement,    // a required child element is missing
    MissingAttribute,  // a required attribute is missing
    BadValue,          // an attribute or text does not hold a valid value
    Export,            // writing an output file failed
    Io,                // any other file-system or I/O failure
};

// Result error of every fallible function in fez_core (used as std::expected<T, Error>).
//  - message: human-readable description (UTF-8)
//  - source:  where it happened (UTF-8): a file path, "<file>:<line>:<column>" for XML syntax errors, or an element
//             path such as "Level/ArtObjects/Entry[3]/ArtObjectInstance@name" for content errors
struct Error
{
    ErrorKind kind;
    std::string message;
    std::string source;
};

constexpr std::string_view toString(ErrorKind kind) noexcept
{
    switch(kind)
    {
    case ErrorKind::FileNotFound:
        return "FileNotFound";
    case ErrorKind::XmlSyntax:
        return "XmlSyntax";
    case ErrorKind::MissingElement:
        return "MissingElement";
    case ErrorKind::MissingAttribute:
        return "MissingAttribute";
    case ErrorKind::BadValue:
        return "BadValue";
    case ErrorKind::Export:
        return "Export";
    case ErrorKind::Io:
        return "Io";
    }
    return "Unknown";
}

} // namespace fez
