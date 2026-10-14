#include "export/ObjExport.h"

#include <QtCore/QDir>
#include <QtCore/QFileInfo>

#include <assimp/Exporter.hpp>
#include <assimp/scene.h>

namespace fez
{

namespace
{

// Creates the absolute, clean path one component at a time, parents first. QDir::mkpath is not used: it failed when
// another thread created a missing parent at the same moment (fez_mode-777.20). Each single QDir::mkdir either
// creates its component or fails; a failure is only an error when the component is not a directory afterwards.
bool makeComponents(const QString& path)
{
    if(QFileInfo(path).isDir())
        return true;

    const auto parent = QFileInfo(path).path();

    if(parent != path && !makeComponents(parent))
        return false;

    return QDir().mkdir(path) || QFileInfo(path).isDir();
}

} // namespace

std::expected<void, Error> makeDirectory(const QString& path)
{
    if(!makeComponents(QDir::cleanPath(QDir(path).absolutePath())))
        return std::unexpected(Error{ErrorKind::Export, "cannot create directory", path.toStdString()});

    return {};
}

std::expected<void, Error> exportObj(const aiScene& scene, const QString& dir, const QString& name)
{
    if(auto created = makeDirectory(dir); !created)
        return created;

    const auto obj_path = dir + "/" + name + ".obj";

    Assimp::Exporter exporter;

    if(exporter.Export(&scene, "obj", obj_path.toStdString()) != aiReturn_SUCCESS)
        return std::unexpected(Error{ErrorKind::Export, exporter.GetErrorString(), obj_path.toStdString()});

    return {};
}

} // namespace fez
