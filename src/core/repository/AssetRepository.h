#pragma once

#include "error/Error.h"
#include "model/Geometry.h"
#include "model/Texture.h"
#include "parser/TrileSetParser.h"

#include <QtCore/QString>

#include <exception>
#include <expected>
#include <functional>
#include <future>
#include <map>
#include <memory>
#include <mutex>
#include <utility>

namespace fez
{

// Loads trile sets, art objects and textures of one FEZ export on demand and keeps them for the whole run.
//
// Thread safety: all lookups may be called from any number of threads at once. Each kind has its own cache of
// std::map<Key, std::shared_future<Result>> guarded by a std::mutex. The mutex is held only to look up or insert the
// future, never while loading: the first requester of a key loads it on its own thread (outside the lock) and
// fulfils the future, later requesters of the same key wait on that future, and requests for other keys are not
// blocked at all. A key is therefore loaded at most once at a time and every waiter of one load gets the same
// pointer or the same Error.
//
// Failures are not cached (as with the static caches of the old LevelParser): when a load fails its entry is removed,
// so the waiters of that load all get its Error and a later request loads the key again. Successful results are kept
// until the repository is destroyed. An exception thrown by a loader reaches every waiter of that load and is not
// cached either.
class AssetRepository
{
public:
    template<typename T>
    using Result = std::expected<std::shared_ptr<const T>, Error>;

    // The functions that read assets from disk; tests inject their own.
    struct Loaders
    {
        std::function<std::expected<TrileSet, Error>(const QString& path)> trileSet;
        std::function<std::expected<Geometry, Error>(const QString& path)> artObject;
        std::function<std::expected<Texture, Error>(const QString& dir, const QString& name, bool animated)> texture;
    };

    // parseTrileSet and parseArtObject (a missing file is FileNotFound without parsing it, so nothing is logged for it,
    // as before), loadTexture.
    static Loaders defaultLoaders();

    // inputRoot is the FEZ export root (contains "trile sets", "art objects", ...); it is made absolute and clean.
    explicit AssetRepository(const QString& inputRoot, Loaders loaders = defaultLoaders());

    AssetRepository(const AssetRepository&) = delete;
    AssetRepository& operator=(const AssetRepository&) = delete;

    // The absolute, clean input root.
    const QString& inputRoot() const noexcept;

    // <root>/trile sets/<name>.xml, keyed by name.
    Result<TrileSet> trileSet(const QString& name);

    // <root>/art objects/<name>.xml, keyed by name.
    Result<Geometry> artObject(const QString& name);

    // loadTexture(<root>/<dir>, name, animated); dir is relative to the root (an absolute dir is used as is) and made
    // absolute and clean. Keyed by "<absolute dir>/<name>": animated is not part of the key, so the first request of
    // a key decides whether a static or an animated texture is loaded (as the old LevelParser texture cache did).
    Result<Texture> texture(const QString& dir, const QString& name, bool animated);

private:
    // Load-once cache of one asset kind (see the class comment).
    template<typename T>
    class Cache
    {
    public:
        template<typename Load>
        Result<T> get(const QString& key, Load&& load)
        {
            std::promise<Result<T>> promise;
            std::shared_future<Result<T>> pending;

            {
                const std::lock_guard lock(m_Mutex);

                if(const auto found = m_Entries.find(key); found != m_Entries.cend())
                    pending = found->second;
                else
                    m_Entries.emplace(key, promise.get_future().share());
            }

            // another thread loads (or loaded) this key: wait outside the lock
            if(pending.valid())
                return pending.get();

            return loadAndPublish(key, promise, std::forward<Load>(load));
        }

    private:
        template<typename Load>
        Result<T> loadAndPublish(const QString& key, std::promise<Result<T>>& promise, Load&& load)
        {
            auto result = Result<T>();

            try
            {
                auto loaded = load();

                if(loaded)
                    result = std::make_shared<const T>(std::move(*loaded));
                else
                    result = std::unexpected(std::move(loaded.error()));
            }
            catch(...)
            {
                forget(key);
                promise.set_exception(std::current_exception());
                throw;
            }

            if(!result)
                forget(key);

            promise.set_value(result);

            return result;
        }

        void forget(const QString& key)
        {
            const std::lock_guard lock(m_Mutex);
            m_Entries.erase(key);
        }

        std::mutex m_Mutex;
        std::map<QString, std::shared_future<Result<T>>> m_Entries;
    };

    QString m_InputRoot;
    Loaders m_Loaders;

    Cache<TrileSet> m_TrileSets;
    Cache<Geometry> m_ArtObjects;
    Cache<Texture> m_Textures;
};

} // namespace fez
