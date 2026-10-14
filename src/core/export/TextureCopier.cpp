#include "export/TextureCopier.h"

#include <QtCore/QDir>
#include <QtCore/QFile>
#include <QtCore/QFileInfo>

namespace fez
{

bool TextureCopier::copy(const QString& source, const QString& destination)
{
    const auto key = QDir::cleanPath(destination);

    {
        const std::lock_guard lock(m_Mutex);

        if(!m_Destinations.insert(key).second)
            return false;
    }

    // as the old Writer::save: create the missing destination directory (one level), copy without overwriting
    const auto texture_out_dir = QDir(QFileInfo(destination).absolutePath());

    if(!texture_out_dir.exists())
        texture_out_dir.mkdir(".");

    QFile texture(source);

    if(texture.copy(destination))
        return true;

    const std::lock_guard lock(m_Mutex);
    m_Destinations.erase(key);

    return false;
}

} // namespace fez
