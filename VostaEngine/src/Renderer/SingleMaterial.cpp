#include "vepch.h"
#include "SingleMaterial.h"
#include "MaterialNodes.h"
#include "ShaderGenerator.h"
#include "Core/Log.h"
#include "Core/ResourceManager.h"

namespace ve {

    bool SingleMaterial::compile() {
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
            customShader = nullptr;
            return false;
        }

        customShader = Shader::create(name, vertSrc, fragSrc);
        if (!customShader) {
            compileError = "OpenGL shader compilation failed";
            isCompiled = false;
            customShader = nullptr;
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

    Ref<Shader> SingleMaterial::getShader() {
        if (!isCompiled && graph.nodes.size() > 1) {
            compile();
        }
        return customShader;
    }

    void SingleMaterial::fillBindings(MaterialBindingSet& out) const {
        out.textures.clear();
        out.ints.clear();
        out.floats.clear();
        out.vec2s.clear();
        out.vec3s.clear();

        // Each channel: a map wins, otherwise the flat fallback param feeds the
        // shader. The u_UseXxxMap flag tells the shader which of the two to read.
        if (albedoMapHandle.isValid()) {
            out.textures.push_back({"u_AlbedoMap", albedoMapHandle, 0});
            out.ints.push_back({"u_UseAlbedoMap", 1});
        } else {
            out.vec3s.push_back({"u_AlbedoColor", albedoColor});
            out.ints.push_back({"u_UseAlbedoMap", 0});
        }

        if (normalMapHandle.isValid()) {
            out.textures.push_back({"u_NormalMap", normalMapHandle, 1});
            out.ints.push_back({"u_UseNormalMap", 1});
        } else {
            out.ints.push_back({"u_UseNormalMap", 0});
        }

        if (metallicMapHandle.isValid()) {
            out.textures.push_back({"u_MetallicMap", metallicMapHandle, 2});
            out.ints.push_back({"u_UseMetallicMap", 1});
        } else {
            out.floats.push_back({"u_Metallic", metallic});
            out.ints.push_back({"u_UseMetallicMap", 0});
        }

        if (roughnessMapHandle.isValid()) {
            out.textures.push_back({"u_RoughnessMap", roughnessMapHandle, 3});
            out.ints.push_back({"u_UseRoughnessMap", 1});
        } else {
            out.floats.push_back({"u_Roughness", roughness});
            out.ints.push_back({"u_UseRoughnessMap", 0});
        }

        if (aoMapHandle.isValid()) {
            out.textures.push_back({"u_AOMap", aoMapHandle, 4});
            out.ints.push_back({"u_UseAOMap", 1});
        } else {
            out.floats.push_back({"u_AO", ao});
            out.ints.push_back({"u_UseAOMap", 0});
        }

        // Graph-compiled sampler nodes fill the units above the fixed PBR set.
        uint32_t unit = kFixedTextureUnits;
        for (const auto& b : graphTextures) {
            out.textures.push_back({b.uniformName, b.textureHandle, unit});
            ++unit;
        }
    }

    // --- Serialization (.veasset) ---
    // Format:
    //   "material"
    //   <name>
    //   <albedoPath> <normalPath> <metallicPath> <roughnessPath> <aoPath> <emissivePath>
    //   <albedoColor> <metallic> <roughness> <ao> <emissiveColor>
    //   <isCompiled>
    //   <graph>

    void SingleMaterial::serialize(Archive& ar) const {
        ar << std::string(kTypeTag);
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

        graph.serialize(getMaterialNodeRegistry(), ar);
    }

    void SingleMaterial::deserialize(Archive& ar) {
        std::string typeTag;
        ar >> typeTag;
        // typeTag should be kTypeTag; ignore if it's not (backwards compat).

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

        graph.deserialize(getMaterialNodeRegistry(), ar);

        // A saved-as-compiled material rebuilds its GL shader (and the graph
        // texture bindings that go with it) now that the graph is loaded.
        if (isCompiled) {
            compile();
        }
    }

}
