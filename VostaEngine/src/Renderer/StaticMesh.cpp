#include "vepch.h"
#include "StaticMesh.h"
#include "Core/AssetConfig.h"
#include "Core/Log.h"
#include "Core/ResourceManager.h"

#include "Platform/OpenGL/OpenGLStaticMesh.h"
#include "Asset/StaticMeshImporter.h"
#include "Asset/Utils.h"
#include "Renderer/Material.h"

namespace ve {

    Ref<StaticMeshResource> StaticMesh::load(const std::string& path) {
        std::string resolvedPath = toAbsolute(path);
        if (utils::getExtension(resolvedPath) != ".veasset") {
            VE_CORE_ERROR_PRINT("StaticMesh async load requires a baked asset: %s", path.c_str());
            return nullptr;
        }
        return StaticMeshImporter::loadFromAsset(resolvedPath);
    }

    Ref<StaticMesh> StaticMesh::upload(Ref<StaticMeshResource> staticMeshResource) {
        if (!staticMeshResource || staticMeshResource->vertexBuffer.empty()) {
            return nullptr;
        }
        for (auto& sub : staticMeshResource->subMeshes) {
            if (!sub.materialHandle.isValid() && !sub.materialPath.empty()) {
                sub.materialHandle = ResourceManager::store<Material>(sub.materialPath);
            }
        }
        return CreateRef<OpenGLStaticMesh>(std::move(staticMeshResource));
    }

    Ref<StaticMesh> StaticMesh::createPlaceholder() {
        return nullptr;
    }

    Ref<StaticMesh> StaticMesh::create(const std::string& path) {

        std::string resolvedPath = toAbsolute(path);

        // A .veasset is a baked mesh: load it directly. Any other extension is
        // a source file (.obj/.fbx) that has to be imported.
        Ref<StaticMeshResource> resource = (utils::getExtension(resolvedPath) == ".veasset")
            ? load(path)
            : StaticMeshImporter::importFromFile(resolvedPath);

        if (!resource || resource->vertexBuffer.empty()) {
            VE_CORE_ERROR_PRINT("Failed to create StaticMesh: %s", path.c_str());
            return nullptr;
        }
        return upload(std::move(resource));
    }

    Ref<StaticMesh> StaticMesh::create(const Ref<StaticMeshResource>& staticMeshResource) {
        return upload(staticMeshResource);
    }
}
