#pragma once

#include "error/Error.h"

#include <QtCore/QString>

#include <expected>
#include <mutex>
#include <set>

namespace fez
{

// Copies textures next to the exported OBJ files; one copier is shared by all exports of a phase (thread-safe).
// Each destination is copied at most once per copier: a level references the same texture from many instances.
class TextureCopier
{
public:
    // Copies source to destination. Missing directories of the destination are created (makeDirectory, safe against
    // other threads creating the same directories), and an existing destination file is replaced, like the OBJ/MTL
    // files: converting into an existing output folder gives the same result as a fresh one. Does nothing and returns
    // false when this copier already copied the destination or another thread is copying it; returns true when this
    // call copied the file. Error kind Io when the source is missing (source: the source path), a directory cannot be
    // created (source: the directory), or the destination cannot be replaced or written (source: the destination).
    // A failed copy is forgotten, so a later call tries again.
    std::expected<bool, Error> copy(const QString& source, const QString& destination);

private:
    std::mutex m_Mutex;
    std::set<QString> m_Destinations; // copied or being copied (clean paths)
};

} // namespace fez
