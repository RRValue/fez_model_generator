#pragma once

#include "error/Error.h"

#include <QtCore/QString>

#include <expected>

struct aiScene;

namespace fez
{

// Creates the directory path and every missing parent. Safe while other threads create the same directory or share a
// missing parent: each missing component is created on its own, and a component that already exists as a directory
// (also when another thread created it first) counts as success. Error kind Export when a component cannot be created.
std::expected<void, Error> makeDirectory(const QString& path);

// Writes the scene as <dir>/<name>.obj with its materials in <dir>/<name>.mtl (assimp OBJ exporter); creates dir via
// makeDirectory first. Error kind Export (source: the directory or the .obj path) when that fails.
std::expected<void, Error> exportObj(const aiScene& scene, const QString& dir, const QString& name);

} // namespace fez
