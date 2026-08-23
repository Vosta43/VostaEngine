#include "vepch.h"
#include "Material.h"
#include "MaterialNodes.h"
#include "ShaderGenerator.h"
#include "Core/Log.h"
#include "Core/AssetConfig.h"
#include "Core/ResourceManager.h"

namespace ve {

    Ref<Material> Material::create(const std::string& path) {
        // Ensure .veasset extension.
        std::string filePath = path;
        if (filePath.size() < 8 || filePath.compare(filePath.size() - 8, 8, ".veasset") != 0) {
            filePath += ".veasset";
        }

        // Try loading existing .veasset file.
        {
            TextArchive ar(filePath, ArchiveMode::read);
            if (ar.isGood()) {
                auto material = CreateRef<Material>();
                material->deserialize(ar);
                if (material->isCompiled) {
                    material->compile();
                }
                return material;
            }
        }

        // Fresh material: add a default output node and save immediately.
        auto material = CreateRef<Material>();
        material->name = toRelative(filePath);
        material->graph.addNode(CreateRef<MaterialOutputNode>());
        {
            TextArchive ar(filePath, ArchiveMode::write);
            if (ar.isGood()) {
                material->serialize(ar);
            }
        }
        return material;
    }

    bool Material::compile() {
        // Empty graph (only the default MaterialOutputNode) -> PBR fallback.
        if (graph.nodes.size() <= 1) {
            isCompiled = false;
            customShader = nullptr;
            return false;
        }

        std::string vertSrc, fragSrc;
        ShaderGenerator::compile(graph, vertSrc, fragSrc);
        if (vertSrc.empty() || fragSrc.empty()) {
            compileError = "ShaderGenerator produced empty source";
            isCompiled = false;
            return false;
        }

        customShader = Shader::create(name, vertSrc, fragSrc);
        if (!customShader) {
            compileError = "OpenGL shader compilation failed";
            isCompiled = false;
            return false;
        }

        // Fill graph texture bindings from TextureSamplerNodes.
        graphTextures.clear();
        for (auto& node : graph.nodes) {
            if (auto* texNode = dynamic_cast<TextureSamplerNode*>(node.get())) {
                if (texNode->textureHandle.isValid()) {
                    graphTextures.push_back({texNode->samplerName, texNode->textureHandle});
                }
            }
        }

        isCompiled = true;
        compileError.clear();
        return true;
    }

    Ref<Shader> Material::getShader() {
        if (!isCompiled && graph.nodes.size() > 1) {
            compile();
        }
        return customShader;
    }

    // --- Serialization (.veasset) ---
    // Format:
    //   "material"
    //   <name>
    //   <albedoPath> <normalPath> <metallicPath> <roughnessPath> <aoPath> <emissivePath>
    //   <albedoColor> <metallic> <roughness> <ao> <emissiveColor>
    //   <isCompiled>
    //   <graph>

    void Material::serialize(Archive& ar) const {
        ar << std::string("material");
        ar << name;

        // PBR texture paths.
        auto texPath = [](AssetHandle h) -> std::string {
            return h.isValid() ? ResourceManager::getPath<Texture2D>(h) : "";
        };
        ar << texPath(albedoMapHandle);
        ar << texPath(normalMapHandle);
        ar << texPath(metallicMapHandle);
        ar << texPath(roughnessMapHandle);
        ar << texPath(aoMapHandle);
        ar << texPath(emissiveMapHandle);

        // PBR fallback colors.
        ar << albedoColor;
        ar << metallic << roughness << ao;
        ar << emissiveColor;

        uint8_t compiled = isCompiled ? 1 : 0;
        ar << compiled;

        graph.serialize(ar);
    }

    void Material::deserialize(Archive& ar) {
        std::string typeTag;
        ar >> typeTag;
        // typeTag should be "material"; ignore if it's not (backwards compat).

        ar >> name;

        // PBR texture paths.
        auto loadTex = [&]() -> AssetHandle {
            std::string p;
            ar >> p;
            return p.empty() ? INVALID_ASSET_HANDLE : ResourceManager::store<Texture2D>(p);
        };
        albedoMapHandle   = loadTex();
        normalMapHandle   = loadTex();
        metallicMapHandle = loadTex();
        roughnessMapHandle = loadTex();
        aoMapHandle       = loadTex();
        emissiveMapHandle = loadTex();

        // PBR fallback colors.
        ar >> albedoColor;
        ar >> metallic >> roughness >> ao;
        ar >> emissiveColor;

        uint8_t compiled = 0;
        ar >> compiled;
        isCompiled = (compiled != 0);
        customShader = nullptr; // OpenGL object, must recompile.

        graph.deserialize(ar);
    }

}
