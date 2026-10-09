#include "vepch.h"
#include "ResourceManager.h"

#include "Renderer/Texture.h"
#include "Renderer/StaticMesh.h"
#include "Renderer/Material.h"
#include "Asset/StaticMeshResource.h"
#include "Noise/NoiseGraphResource.h"
#include "Renderer/MaterialLayerAsset.h"
#include "Scene/Terrain/TerrainDataResource.h"


namespace ve {
    template<typename T>
    ResourceStorage<T>& ResourceManager::storage() {
        static ResourceStorage<T> s;
        return s;
    }
    // Every resource type that the engine loads through ResourceManager
    // must be listed here. The explicit instantiation forces the compiler
    // to emit the storage() body for this type into the current translation
    // unit, so the linker can find it when other .cpp files call
    // ResourceManager::store<T>().
	template VE_API ResourceStorage<Texture2D>& ResourceManager::storage<Texture2D>();
    template VE_API ResourceStorage<TextureCubeMap>& ResourceManager::storage<TextureCubeMap>();
    template VE_API ResourceStorage<StaticMesh>& ResourceManager::storage<StaticMesh>();
    template VE_API ResourceStorage<Material>& ResourceManager::storage<Material>();
    template VE_API ResourceStorage<StaticMeshResource>& ResourceManager::storage<StaticMeshResource>();
    template VE_API ResourceStorage<NoiseGraphResource>& ResourceManager::storage<NoiseGraphResource>();
    template VE_API ResourceStorage<MaterialLayerAsset>& ResourceManager::storage<MaterialLayerAsset>();
    template VE_API ResourceStorage<TerrainDataResource>& ResourceManager::storage<TerrainDataResource>();

    template class  VE_API ResourceStorage<Texture2D>;
    template class  VE_API ResourceStorage<TextureCubeMap>;
    template class  VE_API ResourceStorage<StaticMesh>;
    template struct VE_API ResourceStorage<Material>;
    template class VE_API ResourceStorage<StaticMeshResource>;
    template struct VE_API ResourceStorage<NoiseGraphResource>;
    template struct VE_API ResourceStorage<MaterialLayerAsset>;
    template struct VE_API ResourceStorage<TerrainDataResource>;

    void ResourceManager::renamePrefixAll(const std::string& oldPath, const std::string& newPath) {
        // Keep this list in step with the explicit instantiations above.
        const std::string oldRel = toRelative(oldPath);
        const std::string newRel = toRelative(newPath);
        storage<Texture2D>().renamePrefix(oldRel, newRel);
        storage<TextureCubeMap>().renamePrefix(oldRel, newRel);
        storage<StaticMesh>().renamePrefix(oldRel, newRel);
        storage<Material>().renamePrefix(oldRel, newRel);
        storage<StaticMeshResource>().renamePrefix(oldRel, newRel);
        storage<NoiseGraphResource>().renamePrefix(oldRel, newRel);
        storage<MaterialLayerAsset>().renamePrefix(oldRel, newRel);
        storage<TerrainDataResource>().renamePrefix(oldRel, newRel);
    }

}
