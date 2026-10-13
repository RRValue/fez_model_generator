#include "repository/AssetRepository.h"

#include "parser/ArtObjectParser.h"
#include "parser/TextureParser.h"

#include <QtCore/QDir>
#include <QtCore/QFileInfo>

namespace fez
{

namespace
{
    // FileNotFound without calling the parser (the old LevelParser checked existence before parsing, so a missing
    // file logs nothing).
    template<typename Parse>
    auto parseExisting(const QString& path, Parse parse) -> decltype(parse(path))
    {
        if(!QFileInfo::exists(path))
            return std::unexpected(Error{ErrorKind::FileNotFound, "file not found", path.toStdString()});

        return parse(path);
    }
} // namespace

AssetRepository::Loaders AssetRepository::defaultLoaders()
{
    Loaders loaders;
    loaders.trileSet = [](const QString& path) { return parseExisting(path, parseTrileSet); };
    loaders.artObject = [](const QString& path) { return parseExisting(path, parseArtObject); };
    loaders.texture = [](const QString& dir, const QString& name, bool animated) { return loadTexture(dir, name, animated); };
    return loaders;
}

AssetRepository::AssetRepository(const QString& inputRoot, Loaders loaders)
    : m_InputRoot(QDir(inputRoot).absolutePath())
    , m_Loaders(std::move(loaders))
{
}

const QString& AssetRepository::inputRoot() const noexcept
{
    return m_InputRoot;
}

AssetRepository::Result<TrileSet> AssetRepository::trileSet(const QString& name)
{
    return m_TrileSets.get(name, [this, &name] { return m_Loaders.trileSet(m_InputRoot + "/trile sets/" + name + ".xml"); });
}

AssetRepository::Result<Geometry> AssetRepository::artObject(const QString& name)
{
    return m_ArtObjects.get(name, [this, &name] { return m_Loaders.artObject(m_InputRoot + "/art objects/" + name + ".xml"); });
}

AssetRepository::Result<Texture> AssetRepository::texture(const QString& dir, const QString& name, bool animated)
{
    const auto absolute_dir = QDir(QDir(m_InputRoot).filePath(dir)).absolutePath();

    return m_Textures.get(absolute_dir + "/" + name, [this, &absolute_dir, &name, animated] { return m_Loaders.texture(absolute_dir, name, animated); });
}

} // namespace fez
