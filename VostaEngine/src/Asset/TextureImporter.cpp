#include "vepch.h"
#include "TextureImporter.h"
#include "Core/Log.h"
#include "Scene/Archive.h"

#include <glad/glad.h>
#include "stb_image.h"
#include <fstream>
#include <filesystem>
#include <glm.hpp>

namespace ve {


    Ref<TextureResource> TextureImporter::importFromFile(const std::string& filePath){
        int width, height, channels;

        TextureResource texutre2DResource;
        stbi_set_flip_vertically_on_load(1);

        // HDR path (Radiance .hdr files)
        if (stbi_is_hdr(filePath.c_str())) {
            float* data = stbi_loadf(filePath.c_str(), &width, &height, &channels, 0);

            if (!data) {
                VE_CORE_ERROR_PRINT("Failed to load HDR texture: %s", filePath.c_str());
                return nullptr;
            }

            if (channels == 4) {
                texutre2DResource.format = TextureFormat::RGBA16F;
            }
            else if (channels == 3) {
                texutre2DResource.format = TextureFormat::RGB16F;
            }
            else {
                VE_CORE_ERROR_PRINT("Unsupported HDR channel count (%d): %s", channels, filePath.c_str());
                stbi_image_free(data);
                return nullptr;
            }

            texutre2DResource.sourceFilePath = filePath;
            texutre2DResource.width = static_cast<uint32_t>(width);
            texutre2DResource.height = static_cast<uint32_t>(height);

            size_t dataSize = static_cast<size_t>(width) * height * channels;
            texutre2DResource.floatPixels.resize(dataSize);
            std::memcpy(texutre2DResource.floatPixels.data(), data, dataSize * sizeof(float));

            stbi_image_free(data);
            return CreateRef<TextureResource>(texutre2DResource);
        }

        // LDR path
        unsigned char* data = stbi_load(filePath.c_str(), &width, &height, &channels, 0);

        if (!data) {
            VE_CORE_ERROR_PRINT("Failed to load texture: %s", filePath.c_str());
            return nullptr;
        }

        if (channels == 4) {
            texutre2DResource.format = TextureFormat::RGBA;
        }
        else if (channels == 3) {
            texutre2DResource.format = TextureFormat::RGB;
        }
        else if (channels == 1) {
            texutre2DResource.format = TextureFormat::R8;
        }
        else {
            VE_CORE_ERROR_PRINT("Unsupported channel count (%d): %s", channels, filePath.c_str());
            stbi_image_free(data);
            return nullptr;
        }

        texutre2DResource.sourceFilePath = filePath;

        texutre2DResource.width = static_cast<uint32_t>(width);
        texutre2DResource.height = static_cast<uint32_t>(height);

        size_t dataSize = static_cast<size_t>(width) * height * channels;

        texutre2DResource.pixels.resize(dataSize);
        std::memcpy(texutre2DResource.pixels.data(), data, dataSize);

        stbi_image_free(data);

        return CreateRef<TextureResource>(texutre2DResource);
    }


    // --- Baked texture asset (.veasset, binary) ----------------------------
    //
    // Layout:
    //   [string] "texture"
    //   [int32]  width, height, format
    //   [int32]  pixelBytes + raw LDR pixels  (empty for HDR sources)
    //   [int32]  floatCount + raw HDR floats  (empty for LDR sources)
    //
    // Self-contained: the decoded pixels are baked in, so loading never needs
    // the source image again.

    void TextureImporter::serialize(const TextureResource& resource, const std::string& serializePath) {
        BinaryArchive ar(serializePath, ArchiveMode::write);
        if (!ar.isGood()) {
            VE_CORE_ERROR_PRINT("Failed to open file for serialization: %s", serializePath.c_str());
            return;
        }

        ar << std::string("texture");
        ar << static_cast<int32_t>(resource.width);
        ar << static_cast<int32_t>(resource.height);
        ar << static_cast<int32_t>(resource.format);

        ar << static_cast<int32_t>(resource.pixels.size());
        if (!resource.pixels.empty())
            ar.writeBytes(resource.pixels.data(), resource.pixels.size());

        ar << static_cast<int32_t>(resource.floatPixels.size());
        if (!resource.floatPixels.empty())
            ar.writeBytes(resource.floatPixels.data(), resource.floatPixels.size() * sizeof(float));

        ar.flush();
    }

    Ref<TextureResource> TextureImporter::deserialize(const std::string& serializePath) {
        BinaryArchive ar(serializePath, ArchiveMode::read);
        if (!ar.isGood()) {
            VE_CORE_ERROR_PRINT("Failed to open file for deserialization: %s", serializePath.c_str());
            return nullptr;
        }

        std::string token;
        ar >> token;
        if (token != "texture") {
            VE_CORE_ERROR_PRINT("Not a texture asset (token='%s'): %s", token.c_str(), serializePath.c_str());
            return nullptr;
        }

        auto resource = CreateRef<TextureResource>();

        int32_t width = 0, height = 0, format = 0;
        ar >> width >> height >> format;
        resource->width = static_cast<uint32_t>(width);
        resource->height = static_cast<uint32_t>(height);
        resource->format = static_cast<TextureFormat>(format);

        int32_t pixelBytes = 0;
        ar >> pixelBytes;
        if (pixelBytes > 0) {
            resource->pixels.resize(static_cast<size_t>(pixelBytes));
            ar.readBytes(resource->pixels.data(), static_cast<size_t>(pixelBytes));
        }

        int32_t floatCount = 0;
        ar >> floatCount;
        if (floatCount > 0) {
            resource->floatPixels.resize(static_cast<size_t>(floatCount));
            ar.readBytes(resource->floatPixels.data(), static_cast<size_t>(floatCount) * sizeof(float));
        }

        if (!ar.isGood()) {
            VE_CORE_ERROR_PRINT("Texture asset truncated: %s", serializePath.c_str());
            return nullptr;
        }

        return resource;
    }

    static glm::vec3 faceToDirection(int face, float u, float v) {
    // Map from [0, 1] to [-1, 1]
    float su = 2.0f * u - 1.0f;
    float sv = 2.0f * v - 1.0f;

    glm::vec3 dir;
    switch (face) {
    case 0: // +X right
        dir = glm::vec3( 1.0f, -sv, -su); break;
    case 1: // -X left
        dir = glm::vec3(-1.0f, -sv,  su); break;
    case 2: // +Y top
        dir = glm::vec3( su,  1.0f,  sv); break;
    case 3: // -Y bottom
        dir = glm::vec3( su, -1.0f, -sv); break;
    case 4: // +Z front
        dir = glm::vec3( su, -sv,  1.0f); break;
    case 5: // -Z back
        dir = glm::vec3(-su, -sv, -1.0f); break;
    }
    return glm::normalize(dir);
}

    static void directionToEquirectUV(const glm::vec3& dir, float& u, float& v) {
        // Assumes equirectangular spans horizontally [0, 2*pi]
        // TODO:Replace 3.141 by engine math lib PI
        u = 0.5f + atan2(dir.z, dir.x) / (2.0f * 3.141f);
        v = 0.5f + asin(dir.y) / 3.141f;
    }

    Ref<TextureCubeMapResource> TextureCubeMapImporter::importFromFile(const std::string& filePath) {
        int srcWidth, srcHeight, channels;
        stbi_set_flip_vertically_on_load(1);

        // stbi_loadf handles both HDR and LDR - LDR gets normalized to [0,1]
        float* srcData = stbi_loadf(filePath.c_str(), &srcWidth, &srcHeight, &channels, 0);
        if (!srcData) {
            VE_CORE_ERROR_PRINT("Failed to load cubemap source: %s", filePath.c_str());
            return nullptr;
        }

        auto resource = CreateRef<TextureCubeMapResource>();
        resource->sourceFilePathPath = filePath;
        resource->width = static_cast<uint32_t>(srcWidth);
        resource->height = static_cast<uint32_t>(srcHeight);
        resource->format = (channels == 4) ? TextureFormat::RGBA16F : TextureFormat::RGB16F;

        uint32_t faceSize = srcWidth / 4;
        if (faceSize < 16) faceSize = 16;
        size_t pixelSize = static_cast<size_t>(channels);
        size_t faceDataSize = faceSize * faceSize * pixelSize;

        for (int face = 0; face < 6; ++face) {
            resource->facePixels[face].resize(faceDataSize);

            for (uint32_t y = 0; y < faceSize; ++y) {
                for (uint32_t x = 0; x < faceSize; ++x) {
                    float u = (x + 0.5f) / faceSize;
                    float v = (y + 0.5f) / faceSize;

                    glm::vec3 dir = faceToDirection(face, u, v);
                    float eu, ev;
                    directionToEquirectUV(dir, eu, ev);

                    float sx = eu * srcWidth;
                    float sy = ev * srcHeight;

                    int x0 = static_cast<int>(sx) % srcWidth;
                    int y0 = static_cast<int>(sy) % srcHeight;
                    int x1 = (x0 + 1) % srcWidth;
                    int y1 = (y0 + 1) % srcHeight;
                    float fx = sx - floorf(sx);
                    float fy = sy - floorf(sy);

                    size_t idx = (y * faceSize + x) * pixelSize;
                    for (size_t c = 0; c < pixelSize; ++c) {
                        float s00 = srcData[(y0 * srcWidth + x0) * pixelSize + c];
                        float s10 = srcData[(y0 * srcWidth + x1) * pixelSize + c];
                        float s01 = srcData[(y1 * srcWidth + x0) * pixelSize + c];
                        float s11 = srcData[(y1 * srcWidth + x1) * pixelSize + c];

                        float val = s00 * (1.0f - fx) * (1.0f - fy)
                            + s10 * fx * (1.0f - fy)
                            + s01 * (1.0f - fx) * fy
                            + s11 * fx * fy;

                        resource->facePixels[face][idx + c] = val;
                    }
                }
            }
        }

        stbi_image_free(srcData);
        return resource;
    }


    void TextureCubeMapImporter::serialize(const TextureCubeMapResource& resource, const std::string& serializePath) {
        // Reserve for offline processing pipeline
    }

    Ref<TextureCubeMapResource> TextureCubeMapImporter::deserialize(const std::string& serializePath) {
        // Reserve for offline processing pipeline
        return nullptr;
    }


}
