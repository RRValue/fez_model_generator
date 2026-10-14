#include "export/TextureCopier.h"

#include "export/ObjExport.h"

#include <QtCore/QDir>
#include <QtCore/QFile>
#include <QtCore/QFileInfo>

#include <string>
#include <utility>

namespace fez
{

namespace
{

Error ioError(const std::string& message, const QString& source)
{
    return Error{ErrorKind::Io, message, source.toStdString()};
}

// The copy itself: creates the destination's directories, removes an existing destination file and copies.
std::expected<void, Error> copyReplacing(const QString& source, const QString& destination)
{
    if(!QFileInfo(source).isFile())
        return std::unexpected(ioError("texture source does not exist", source));

    const auto directory = QFileInfo(destination).absolutePath();

    if(!makeDirectory(directory))
        return std::unexpected(ioError("cannot create texture directory", directory));

    // QFile::copy never overwrites; QFile::remove only removes files (a directory at the destination stays an error)
    if(QFileInfo::exists(destination) && !QFile::remove(destination))
        return std::unexpected(ioError("cannot replace existing texture", destination));

    QFile texture(source);

    if(!texture.copy(destination))
        return std::unexpected(ioError("cannot copy texture from " + source.toStdString() + ": " + texture.errorString().toStdString(), destination));

    return {};
}

} // namespace

std::expected<bool, Error> TextureCopier::copy(const QString& source, const QString& destination)
{
    const auto key = QDir::cleanPath(destination);

    {
        const std::lock_guard lock(m_Mutex);

        if(!m_Destinations.insert(key).second)
            return false;
    }

    // outside the lock: this thread owns the destination until it is released after a failure
    if(auto copied = copyReplacing(source, destination); !copied)
    {
        const std::lock_guard lock(m_Mutex);
        m_Destinations.erase(key);

        return std::unexpected(std::move(copied.error()));
    }

    return true;
}

} // namespace fez
