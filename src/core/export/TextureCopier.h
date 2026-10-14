#pragma once

#include <QtCore/QString>

#include <mutex>
#include <set>

namespace fez
{

// Copies textures next to the exported OBJ files; one copier is shared by all exports of a run (thread-safe).
// Each destination is copied at most once per copier: a level references the same texture from many instances.
class TextureCopier
{
public:
    // Copies source to destination with QFile::copy, which never overwrites an existing file. When the destination's
    // directory is missing, only that one directory is created (QDir::mkdir, not mkpath; kept behavior). Does nothing
    // when this copier already copied the destination or another thread is copying it. A failed copy is forgotten, so
    // a later call tries again. Returns true when this call copied the file. Copy failures are not reported (Task 32).
    bool copy(const QString& source, const QString& destination);

private:
    std::mutex m_Mutex;
    std::set<QString> m_Destinations; // copied or being copied (clean paths)
};

} // namespace fez
