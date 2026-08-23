#include "vepch.h"
#include "StaticMesh.h"
#include "Core/AssetConfig.h"
#include "Core/Log.h"

#include "Platform/OpenGL/OpenGLStaticMesh.h"
#include "Asset/StaticMeshImporter.h"

namespace ve {

    Ref<StaticMesh> StaticMesh::create(const std::string& path){

        std::string resolvedPath = toAbsolute(path);
        auto resource = StaticMeshImporter::importFromFile(resolvedPath);
        if (!resource || resource->vertexBuffer.empty()) {
            VE_CORE_ERROR_PRINT("Failed to create StaticMesh: %s", path.c_str());
            return nullptr;
        }
        return CreateRef<OpenGLStaticMesh>(resource);
    }
    Ref<StaticMesh> StaticMesh::create(const Ref<StaticMeshResource>& staticMeshResource){

        if (!staticMeshResource || staticMeshResource->vertexBuffer.empty()) {

            return nullptr;
        }

        return CreateRef<OpenGLStaticMesh>(staticMeshResource);
    }
}
