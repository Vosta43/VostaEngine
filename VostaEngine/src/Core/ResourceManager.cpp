#include "vepch.h"
#include "ResourceManager.h"

#include "Renderer/Texture.h"
#include "Renderer/StaticMesh.h"
#include "Renderer/Material.h"
#include "Asset/StaticMeshResource.h"
#include "Noise/NoiseGraphResource.h"
#include "Geometry/GeometryGraphResource.h"
#include "Terrain/RuleGraphResource.h"
#include "Renderer/MaterialLayerAsset.h"
#include "Scene/Terrain/TerrainDataResource.h"
#include "Gui/Hud/HudAsset.h"


namespace ve {
    namespace {
        bool s_asyncLoadingEnabled = true;
        size_t s_residencyBudget = 0;
    }

    void ResourceManager::setAsyncLoadingEnabled(bool enabled) {
        s_asyncLoadingEnabled = enabled;
    }

    bool ResourceManager::isAsyncLoadingEnabled() {
        return s_asyncLoadingEnabled;
    }

    std::vector<ResourceManager::StorageOps>& ResourceManager::storageOps() {
        // Leaked: storages register from their own function-local statics, so
        // this must outlive every one of them.
        static auto* ops = new std::vector<StorageOps>();
        return *ops;
    }

    template<typename T>
    ResourceStorage<T>& ResourceManager::storage() {
        static ResourceStorage<T> s;
        // Publish this storage's type-erased view exactly once. Types that are
        // never touched have nothing to rename, evict, or report.
        static const bool registered = [] {
            StorageOps ops;
            ops.renamePrefix = [](const std::string& oldPath, const std::string& newPath) {
                storage<T>().renamePrefix(oldPath, newPath);
            };
            ops.evictLRU = [](size_t bytesToFree, uint64_t now, uint64_t minAge) {
                return storage<T>().evictLRU(bytesToFree, now, minAge);
            };
            ops.residentBytes = [] { return storage<T>().residentBytes(); };
            ops.residentCount = [] { return storage<T>().residentCount(); };
            ops.registeredCount = [] { return storage<T>().registeredCount(); };
            ops.wantedCount = [] { return storage<T>().wantedCount(); };
            ops.reloadWanted = [] { ResourceManager::reloadWanted<T>(storage<T>()); };
            ops.reloadable = supportsPathCreate<T>::value;
            storageOps().push_back(std::move(ops));
            return true;
        }();
        (void)registered;
        return s;
    }

    void ResourceManager::tick() {
        ResourceLifeTimeTickManager& clock = ResourceLifeTimeTickManager::get();

        if (const size_t budget = residencyBudget(); budget > 0) {
            size_t resident = 0;
            for (const StorageOps& op : storageOps())
                resident += op.residentBytes();

            if (resident > budget) {
                // Evict down to 90% of the budget so the next frame does not
                // immediately evict again.
                const size_t lowWater = budget - budget / 10;
                const size_t target = resident - lowWater;
                constexpr uint64_t kMinAgeTicks = 60;
                size_t freed = 0;
                for (const StorageOps& op : storageOps()) {
                    if (freed >= target) break;
                    // Procedural payloads have no path factory, so evicting
                    // them would destroy data that cannot come back.
                    if (!op.reloadable) continue;
                    freed += op.evictLRU(target - freed, clock.resourceClock(), kMinAgeTicks);
                }
            }
        }

        clock.tick();

        // After eviction, so slots touched this frame come back.
        for (const StorageOps& op : storageOps())
            op.reloadWanted();
    }

    ResourceManager::Stats ResourceManager::stats() {
        Stats result;
        for (const StorageOps& op : storageOps()) {
            result.registered += op.registeredCount();
            result.resident += op.residentCount();
            result.bytes += op.residentBytes();
            result.wanted += op.wantedCount();
        }
        return result;
    }

    void ResourceManager::setResidencyBudget(size_t bytes) {
        s_residencyBudget = bytes;
    }

    size_t ResourceManager::residencyBudget() {
        return s_residencyBudget;
    }

    #define VE_DEFINE_RESOURCE_STORAGE(T)                                      \
        template VE_API ResourceStorage<T>& ResourceManager::storage<T>();     \
        template class VE_API ResourceStorage<T>;     

    // Every resource type that the engine loads through ResourceManager
    // must be listed here. The explicit instantiation forces the compiler
    // to emit the storage() body for this type into the current translation
    // unit, so the linker can find it when other .cpp files call
    // ResourceManager::store<T>().
    VE_DEFINE_RESOURCE_STORAGE(Texture2D)
    VE_DEFINE_RESOURCE_STORAGE(Texture2DArray)
    VE_DEFINE_RESOURCE_STORAGE(TextureCubeMap)
    VE_DEFINE_RESOURCE_STORAGE(StaticMesh)
    VE_DEFINE_RESOURCE_STORAGE(Material)
    VE_DEFINE_RESOURCE_STORAGE(StaticMeshResource)
    VE_DEFINE_RESOURCE_STORAGE(NoiseGraphResource)
    VE_DEFINE_RESOURCE_STORAGE(GeometryGraphResource)
    VE_DEFINE_RESOURCE_STORAGE(TerrainRuleGraphResource)
    VE_DEFINE_RESOURCE_STORAGE(MaterialLayerAsset)
    VE_DEFINE_RESOURCE_STORAGE(TerrainDataResource)
    VE_DEFINE_RESOURCE_STORAGE(HudAsset)

    template VE_API AssetHandle ResourceManager::requestAsync<Texture2D>(const std::string&);
    template VE_API AssetHandle ResourceManager::requestAsync<StaticMesh>(const std::string&);


    void ResourceManager::renamePrefixAll(const std::string& oldPath, const std::string& newPath) {
        const std::string oldRel = toRelative(oldPath);
        const std::string newRel = toRelative(newPath);
        for (const StorageOps& ops : storageOps())
            ops.renamePrefix(oldRel, newRel);
    }

    ResourceLifeTimeTickManager& ResourceLifeTimeTickManager::get() {

        static ResourceLifeTimeTickManager* manager = new ResourceLifeTimeTickManager();
        return *manager;
    }

    void ResourceLifeTimeTickManager::tick() {
        m_clock++;
    }

    uint64_t ResourceLifeTimeTickManager::resourceClock() const {
        return m_clock;
    }

}
