#pragma once
#include "ResourceStorage.h"
#include "AssetConfig.h"
#include "Core/AssetLoadQueue.h"
#include "Core/JobSystem.h"
#include <functional>
#include "Log.h"

#include <cstddef>
#include <type_traits>
#include <typeindex>
#include <utility>
#include <vector>

namespace ve {
    
    // Global resource manager facade. Provides a single entry point for loading,
    // querying, and unloading engine resources (textures, meshes, shaders, etc.).
    //
    // Each resource type T gets its own ResourceStorage<T> instance via a Meyer's
    // Singleton (function-local static). The storage is lazily created on first
    // access — if a resource type is never used, it costs zero memory.
    //
    // All methods are static templates. The type T must implement:
    //   - static Ref<T> create(const std::string& path)  → factory
    //   - virtual ~T() or proper RAII cleanup
    //

    template<typename T, typename = void>
    struct supportsAsyncLoad : std::false_type {};

    template<typename T>
    struct supportsAsyncLoad<T, std::void_t<
        decltype(T::createPlaceholder()),
        decltype(T::load(std::declval<const std::string&>())),
        decltype(T::upload(std::declval<decltype(T::load(std::declval<const std::string&>()))>()))
    >> : std::true_type {};

    // Whether a storage can rebuild an entry from its path after eviction.
    // TODO: procedural payloads (weight-map arrays, baked mesh buffers) may not
    // belong in the path-keyed registry at all; revisit when streaming lands.
    template<typename T, typename = void>
    struct supportsPathCreate : std::false_type {};

    template<typename T>
    struct supportsPathCreate<T, std::void_t<
        decltype(T::create(std::declval<const std::string&>()))
    >> : std::true_type {};


    class VE_API ResourceManager {
    public:
        static void setAsyncLoadingEnabled(bool enabled);
        static bool isAsyncLoadingEnabled();

        // Advances the shared LRU clock and runs residency maintenance: evicts
        // over budget, then re-issues loads for slots demanded while unloaded.
        // Call once per frame on the main thread, after AssetLoadQueue::pump().
        static void tick();

        // Resident byte budget across all types. 0 (the default) disables
        // eviction.
        static void setResidencyBudget(size_t bytes);
        static size_t residencyBudget();

        struct Stats {
            size_t registered = 0;  // slots owning a path, loaded or not
            size_t resident = 0;    // slots holding a payload
            size_t bytes = 0;       // estimated resident bytes
            size_t wanted = 0;      // unloaded slots with outstanding demand
        };
        static Stats stats();

        // Load a resource from disk and store it. If the path was already loaded,
        // the existing handle is returned (deduplication). Returns
        // INVALID_ASSET_HANDLE if T::create(path) fails (file not found, parse
        // error, etc.).
        template<typename T>
        static AssetHandle store(const std::string& path) {
            std::string relativePath = toRelative(path);
            auto& st = storage<T>();
            // Resolve before creating: re-importing an already-registered path
            // would build a throwaway resource and then discard it, and paths
            // that have no file behind them (built-in "__builtin_..." keys) can
            // only be resolved this way.
            AssetHandle existing = st.find(relativePath);
            if (existing.isValid() && !st.isUnloaded(existing)) return existing;

            Ref<T> resource = T::create(toAbsolute(relativePath));
            if (!resource) return INVALID_ASSET_HANDLE;
            // store() refills an unloaded slot in place, keeping the handle.
            return st.store(relativePath, resource);
        }
        
        // Kick off a background load and return a handle immediately. The slot
        // holds a placeholder until the worker finishes; the upload and install
        // run on the main thread from AssetLoadQueue::pump().
        template<typename T>
        static AssetHandle requestAsync(const std::string& path) {
            const std::string rel = toRelative(path);

            auto& st = getStorage<T>();
            AssetHandle existing = st.find(rel);
            if (existing.isValid() && !st.isUnloaded(existing)) {
                return existing;
            }

            // storeLoading() refills an unloaded slot in place, so a handle
            // obtained before eviction keeps resolving to the placeholder.
            AssetHandle handle = st.storeLoading(rel, T::createPlaceholder());
            if (!handle.isValid()) {
                return INVALID_ASSET_HANDLE;
            }

            scheduleLoad<T>(rel, handle);
            return handle;
        }

        template<typename T>
        static AssetHandle load(const std::string& path) {
            if constexpr (supportsAsyncLoad<T>::value) {
                if (isAsyncLoadingEnabled()) {
                    return requestAsync<T>(path);
                }
            }
            return store<T>(path);
        }

        // Retrieve a resource by handle. Returns null if the handle is stale
        // (its slot was removed and reused) or if its slot is currently
        // unloaded; an unloaded slot records the demand for reload-on-tick.
        template<typename T>
        static Ref<T> get(AssetHandle handle) {
            return storage<T>().get(handle);
        }

        template<typename T>
        static auto& getStorage() {
            return storage<T>();  
        }

        template<typename T>
        static std::string getPathByHandle(AssetHandle handle) {
            auto& storage = getStorage<T>();
            return storage.getPath(handle);
        }

        template<typename T>
        static AssetHandle find(const std::string& path) {
            return storage<T>().find(toRelative(path));
        }

        template<typename T>
        static const std::string& getPath(AssetHandle handle) {
            return storage<T>().getPath(handle);
        }

        template<typename T>
        static bool contains(AssetHandle handle) {
            return storage<T>().contains(handle);
        }

        template<typename T>
        static bool contains(const std::string& path) {
            return storage<T>().contains(toRelative(path));
        }

        template<typename T>
        static void remove(AssetHandle handle) {
            storage<T>().remove(handle);
        }

        template<typename T>
        static void remove(const std::string& path) {
            storage<T>().remove(toRelative(path));
        }

        template<typename T>
        static size_t count() {
            return storage<T>().size();
        }

        template<typename T>
        static void forEach(std::function<void(AssetHandle, const std::string&)> callback) {
            for (auto& [path, handle] : storage<T>().entries()) {
                callback(handle, path);
            }
        }

        // Re-key every loaded resource of every type under `oldPath` (a single
        // file or a folder prefix) to `newPath`, so a rename/move on disk keeps
        // the in-memory resources — and any handles pointing at them — valid.
        static void renamePrefixAll(const std::string& oldPath, const std::string& newPath);

    private:
        // Cooldown before demand may re-issue a failed load (~10 s at 60 fps).
        static constexpr uint64_t kLoadRetryCooldownTicks = 600;

        // Type-erased handles onto each ResourceStorage<T>, registered the
        // first time a type is used. Gives the non-template entry points
        // (tick, stats, renamePrefixAll) one uniform view without a virtual
        // base on the storage template.
        struct StorageOps {
            std::function<void(const std::string&, const std::string&)> renamePrefix;
            std::function<size_t(size_t, uint64_t, uint64_t)> evictLRU;
            std::function<size_t()> residentBytes;
            std::function<size_t()> residentCount;
            std::function<size_t()> registeredCount;
            std::function<size_t()> wantedCount;
            std::function<void()> reloadWanted;
            bool reloadable = false;
        };

        static std::vector<StorageOps>& storageOps();

        template<typename T>
        static void scheduleLoad(const std::string& rel, AssetHandle handle) {
            AssetLoadQueue::get().track(handle);

            const std::string abs = toAbsolute(rel);
            JobSystem::get().schedule([abs, handle] {
                // The job system stores exceptions in a future nobody reads, so
                // complete() would never run; fail the load here instead.
                decltype(T::load(abs)) source = nullptr;
                try {
                    source = T::load(abs);
                }
                catch (...) {
                    source = nullptr;
                }

                AssetLoadQueue::get().complete(handle, [handle, source = std::move(source)]() mutable -> bool {
                    auto& st = getStorage<T>();
                    const uint64_t retryAt = ResourceLifeTimeTickManager::get().resourceClock()
                        + kLoadRetryCooldownTicks;
                    if (!source) {
                        st.fail(handle, retryAt);
                        return false;
                    }
                    Ref<T> resource = T::upload(std::move(source));
                    if (!resource || !st.replace(handle, std::move(resource))) {
                        // replace fails when the slot was removed while the load
                        // was in flight; fail() is a no-op in that case.
                        st.fail(handle, retryAt);
                        return false;
                    }
                    return true;
                });
            });
        }

        // Re-issues loads for slots that get() found unloaded, keeping their
        // handles: async where the type supports it, synchronously otherwise.
        template<typename T>
        static void reloadWanted(ResourceStorage<T>& st) {
            // The whole body must sit inside the discarded branch: anything
            // after an if constexpr is instantiated regardless.
            if constexpr (supportsPathCreate<T>::value) {
                for (const std::string& rel : st.takeWantedPaths()) {
                    AssetHandle handle = st.find(rel);
                    if (!handle.isValid() || !st.isUnloaded(handle)) continue;

                    if constexpr (supportsAsyncLoad<T>::value) {
                        if (isAsyncLoadingEnabled()) {
                            st.markLoading(handle, T::createPlaceholder());
                            scheduleLoad<T>(rel, handle);
                            continue;
                        }
                    }

                    Ref<T> resource = T::create(toAbsolute(rel));
                    if (!resource) {
                        st.fail(handle, ResourceLifeTimeTickManager::get().resourceClock()
                            + kLoadRetryCooldownTicks);
                        continue;
                    }
                    st.replace(handle, std::move(resource));
                }
            }
        }

        // Implementation is in ResourceManager.cpp; explicit template
        // instantiations for all supported resource types must be added there.
        // WHY IS THE BODY IN THE .CPP?
        // Template bodies are normally in headers, but that causes each consuming
        // DLL/EXE to instantiate its own copy of the function-local static,
        // using explicit instantiation with VE_API, we guarantee exactly one
        // instance lives in Engine.dll. All other modules resolve to it via the
        // exported symbol.
        template<typename T>
        static ResourceStorage<T>& storage();  

    };

}