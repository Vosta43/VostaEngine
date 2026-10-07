#include "vepch.h"
#include "Texture.h"

#include "Platform/OpenGL/OpenGLTexture.h"
#include "Asset/TextureResource.h"
#include "Asset/TextureImporter.h"
#include "Core/ResourceManager.h"
#include "Core/AssetConfig.h"
#include "Core/Log.h"
#include "Asset/Utils.h"

namespace ve {
    Ref<Texture2D> Texture2D::create(const std::string& path) {

        std::string resolvedPath = toAbsolute(path);
        Ref<TextureResource> tr = nullptr;

        if (utils::isEngineAsset(resolvedPath)) {
            tr = TextureImporter::deserialize(resolvedPath);
        }
        else {
            tr = TextureImporter::importFromFile(resolvedPath);
        }
        if (!tr) {
            VE_CORE_ERROR_PRINT("Failed to import %s", path.c_str());
            return nullptr;
        }

        // TODO: switch the graphicAPI.But it is not an important job yet
        return CreateRef<OpenGLTexture2D>(tr);
    }
    Ref<Texture2D> Texture2D::create(Ref<TextureResource> resource){

        return CreateRef<OpenGLTexture2D>(resource);
    }
    Ref<Texture2D> Texture2D::create(uint32_t width, uint32_t height){
        // TODO: switch the graphicAPI.But it is not an important job yet
        return CreateRef<OpenGLTexture2D>(width,height);
    }

    Ref<Texture2D> Texture2D::create(uint32_t rendererID, uint32_t width, uint32_t height) {
        return CreateRef<OpenGLTexture2D>(rendererID, width, height, false);
    }

    Ref<Texture2D> Texture2D::create(uint32_t width, uint32_t height, TextureFormat format) {
        return CreateRef<OpenGLTexture2D>(width, height, format);
    }


    Ref<TextureCubeMap> TextureCubeMap::create(const std::string& path) {
        std::string resolvedPath = toAbsolute(path);
        Ref<TextureCubeMapResource> resource = nullptr;
        if (utils::isEngineAsset(resolvedPath)) {
            resource = TextureCubeMapImporter::deserialize(resolvedPath);
        }
        else {
            resource = TextureCubeMapImporter::importFromFile(resolvedPath);
        }
        if (!resource) return nullptr;
        return CreateRef<OpenGLTextureCube>(resource);
    }

    Ref<TextureCubeMap> TextureCubeMap::create(uint32_t faceSize) {
        return CreateRef<OpenGLTextureCube>(faceSize);
    }

    Ref<TextureCubeMap> TextureCubeMap::create(uint32_t faceSize, uint32_t mipLevels) {
        return CreateRef<OpenGLTextureCube>(faceSize, mipLevels);
    }

    Ref<Texture3D> Texture3D::create(uint32_t width, uint32_t height, uint32_t depth, const float* data) {
        // TODO: switch the graphicAPI. But it is not an important job yet
        return CreateRef<OpenGLTexture3D>(width, height, depth, data);
    }

    Ref<Texture3D> Texture3D::create(uint32_t width, uint32_t height, uint32_t depth, TextureFormat format, const float* data, bool repeatWrap) {
        // TODO: switch the graphicAPI. But it is not an important job yet
        return CreateRef<OpenGLTexture3D>(width, height, depth, format, data, repeatWrap);
    }

}
