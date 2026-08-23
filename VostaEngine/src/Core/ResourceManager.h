#pragma once
#include "ResourceStorage.h"
#include "AssetConfig.h"
#include <functional>
#include "Log.h"

#include <typeindex>

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

    class VE_API ResourceManager {
    public:
        // Load a resource from disk and store it. If the path was already loaded,
        // the existing handle is returned (deduplication). Returns
        // INVALID_ASSET_HANDLE if T::create(path) fails (file not found, parse
        // error, etc.).
        template<typename T>
        static AssetHandle store(const std::string& path) {
            std::string relativePath = toRelative(path);
            std::string absolutePath = toAbsolute(relativePath);
            Ref<T> resource = T::create(absolutePath);
            auto& st = storage<T>();
            if (!resource) return INVALID_ASSET_HANDLE;
            return st.store(relativePath, resource);
        }
        // Retrieve a resource by handle. The handle's generation must match
        // the stored entry's generation; returns null if the handle is stale
        // (resource was unloaded and the slot reused).
        template<typename T>
        static Ref<T> get(AssetHandle handle) {
            auto& st = storage<T>();
            //VE_CORE_WARN_PRINT("GET: storage address = %p", (void*)&st);
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

    private:
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