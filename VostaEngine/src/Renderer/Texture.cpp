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

        Ref<TextureResource> tr = load(path);
        if (!tr) return nullptr;

        // TODO: switch the graphicAPI.But it is not an important job yet
        return upload(std::move(tr));
    }
    Ref<TextureResource> Texture2D::load(const std::string& path) {
        std::string resolvedPath = toAbsolute(path);
        Ref<TextureResource> tr;

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

        return tr;
    }

    Ref<Texture2D> Texture2D::upload(Ref<TextureResource> textureResource)
    {
        // TODO: switch the graphicAPI.But it is not an important job yet
        return CreateRef<OpenGLTexture2D>(std::move(textureResource));
    }

    Ref<Texture2D> Texture2D::createPlaceholder() {
        Ref<Texture2D> placeholder = create(1u, 1u);
        uint8_t white[4] = { 255, 255, 255, 255 };
        placeholder->setData(white, sizeof(white));
        return placeholder;
    }


    Ref<Texture2D> Texture2D::create(Ref<TextureResource> resource){

        return upload(std::move(resource));
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

    Ref<Texture2D> Texture2D::create(uint32_t width, uint32_t height, TextureFormat format, uint32_t mipLevels) {
        return CreateRef<OpenGLTexture2D>(width, height, format, mipLevels);
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

    Ref<Texture2DArray> Texture2DArray::create(uint32_t width, uint32_t height, uint32_t layers,
        TextureFormat format, const void* data) {
        // TODO: switch the graphicAPI. But it is not an important job yet
        return CreateRef<OpenGLTexture2DArray>(width, height, layers, format, data);
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
