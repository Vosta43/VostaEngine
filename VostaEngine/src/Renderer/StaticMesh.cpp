#include "vepch.h"
#include "StaticMesh.h"
#include "Core/AssetConfig.h"
#include "Core/Log.h"

#include "Platform/OpenGL/OpenGLStaticMesh.h"
#include "Asset/StaticMeshImporter.h"
#include "Asset/Utils.h"

namespace ve {

    Ref<StaticMesh> StaticMesh::create(const std::string& path){

        std::string resolvedPath = toAbsolute(path);

        // A .veasset is a baked mesh: load it directly. Any other extension is
        // a source file (.obj/.fbx) that has to be imported.
        Ref<StaticMeshResource> resource = (utils::getExtension(resolvedPath) == ".veasset")
            ? StaticMeshImporter::loadFromAsset(resolvedPath)
            : StaticMeshImporter::importFromFile(resolvedPath);

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
