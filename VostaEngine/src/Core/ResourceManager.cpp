#include "vepch.h"
#include "ResourceManager.h"

#include "Renderer/Texture.h"
#include "Renderer/StaticMesh.h"
#include "Renderer/Material.h"
#include "Asset/StaticMeshResource.h"


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

    template class  VE_API ResourceStorage<Texture2D>;
    template class  VE_API ResourceStorage<TextureCubeMap>;
    template class  VE_API ResourceStorage<StaticMesh>;
    template struct VE_API ResourceStorage<Material>;
    template class VE_API ResourceStorage<StaticMeshResource>;

}
