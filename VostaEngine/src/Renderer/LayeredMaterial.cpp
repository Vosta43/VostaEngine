#include "vepch.h"
#include "LayeredMaterial.h"
#include "Asset/BuiltinReousrces.h"
#include "Core/Application.h"
#include "Core/AssetConfig.h"
#include "Core/Log.h"
#include "Core/ResourceManager.h"
#include "Renderer/MaterialLayerAsset.h"

#include <algorithm>
#include <array>

namespace ve {

	namespace {

		std::string withVeassetExt(const std::string& path) {
			if (path.size() < 8 || path.compare(path.size() - 8, 8, ".veasset") != 0)
				return path + ".veasset";
			return path;
		}

		// GLSL array uniform element name, e.g. "u_LayerTiling[2]".
		std::string indexed(const char* base, int i) {
			return std::string(base) + "[" + std::to_string(i) + "]";
		}

		std::string layerPath(AssetHandle h) {
			return h.isValid() ? ResourceManager::getPath<MaterialLayerAsset>(h) : std::string();
		}

		AssetHandle loadLayer(Archive& ar) {
			std::string p;
			ar >> p;
			return p.empty() ? INVALID_ASSET_HANDLE : ResourceManager::store<MaterialLayerAsset>(p);
		}

	} // namespace

	Ref<Shader> LayeredMaterial::getShader() {
		if (!m_shader && !m_shaderLookupDone) {
			m_shader = Application::get().getShaderLibrary().get(kShaderName);
			m_shaderLookupDone = true;
		}
		return m_shader;
	}

	void LayeredMaterial::fillBindings(MaterialBindingSet& out) const {
		out.textures.clear();
		out.ints.clear();
		out.floats.clear();
		out.vec2s.clear();
		out.vec3s.clear();

		const int count = (int)std::min(layers.size(), (size_t)kMaxMaterialLayers);
		out.ints.push_back({"u_LayerCount", count});

		// Resolve the referenced layer assets once. The Refs are held for the whole
		// call so the raw MaterialLayer pointers below stay valid.
		std::vector<Ref<MaterialLayerAsset>> resolved(count);
		for (int i = 0; i < count; ++i)
			resolved[i] = ResourceManager::get<MaterialLayerAsset>(layers[i]);

		auto layerAt = [&](int i) -> const MaterialLayer* {
			return (i >= 0 && i < count && resolved[i]) ? &resolved[i]->layer : nullptr;
		};

		// The shader declares the layer samplers as fixed-size arrays, so GL needs
		// a distinct unit for every element of each array — including slots past
		// u_LayerCount, which are never sampled. Fill those from the first layer
		// that produced a valid pack.
		//
		// Each layer's four logical maps are packed here into the two channel-packed
		// samplers the shader expects (albedo a = height, normal a = roughness). The
		// pack is cached on the layer asset and rebuilt only when a source changes.
		std::array<AssetHandle, kMaxMaterialLayers> packedAlbedo, packedNormal;
		AssetHandle albedoPad, normalPad;
		for (int i = 0; i < count; ++i) {
			if (!resolved[i]) continue;
			const MaterialLayerAsset::PackedTextures& p = resolved[i]->packedTextures();
			packedAlbedo[i] = p.albedoHeight;
			packedNormal[i] = p.normalRoughness;
			if (!albedoPad.isValid() && packedAlbedo[i].isValid()) albedoPad = packedAlbedo[i];
			if (!normalPad.isValid() && packedNormal[i].isValid()) normalPad = packedNormal[i];
		}

		for (int i = 0; i < kMaxMaterialLayers; ++i) {
			AssetHandle albedo = packedAlbedo[i].isValid() ? packedAlbedo[i] : albedoPad;
			AssetHandle normal = packedNormal[i].isValid() ? packedNormal[i] : normalPad;

			if (albedo.isValid())
				out.textures.push_back({indexed("u_LayerAlbedo", i), albedo, (uint32_t)i});
			if (normal.isValid())
				out.textures.push_back({indexed("u_LayerNormal", i), normal, (uint32_t)(kMaxMaterialLayers + i)});
		}

		// Empty slots still declare their scalars so the shader reads defaults
		// rather than whatever a previous draw left bound.
		static const MaterialLayer kDefaultLayer;

		for (int i = 0; i < count; ++i) {
			const MaterialLayer* lp = layerAt(i);
			const MaterialLayer& layer = lp ? *lp : kDefaultLayer;
			out.floats.push_back({indexed("u_LayerTiling", i), layer.tiling});
			out.ints.push_back({indexed("u_LayerBlend", i), (int)layer.blend});
			out.ints.push_back({indexed("u_LayerUseSlope", i), layer.useSlope ? 1 : 0});
			out.vec2s.push_back({indexed("u_LayerSlopeRange", i), glm::vec2(layer.slopeMin, layer.slopeMax)});
			out.ints.push_back({indexed("u_LayerUseHeight", i), layer.useHeight ? 1 : 0});
			out.vec2s.push_back({indexed("u_LayerHeightRange", i), glm::vec2(layer.heightMin, layer.heightMax)});
			out.floats.push_back({indexed("u_LayerNoiseStrength", i), layer.noiseStrength});
			out.floats.push_back({indexed("u_LayerNoiseScale", i), layer.noiseScale});
		}

		out.floats.push_back({"u_HeightScale", heightScale});
		out.floats.push_back({"u_MacroStrength", macroStrength});
		out.floats.push_back({"u_MacroScale", macroScale});

		// Per-terrain control map, declared here so a MaterialInstance can override
		// it by name. Defaults are chosen so a non-instanced material is unchanged:
		// a 1x1 white map with invSize 0 collapses the mask sample to 1. Unit 8
		// follows the layer samplers (albedo 0..3, normal 4..7).
		out.textures.push_back({"u_Weights", BuiltinResources::getDefaultWhiteTexture(), 8});
		out.vec2s.push_back({"u_TerrainOrigin", glm::vec2(0.0f)});
		out.floats.push_back({"u_TerrainInvSize", 0.0f});
	}

	// --- Serialization (.veasset) ---
	//   "layered_material" <version>
	//   <name> <heightScale> <macroStrength> <macroScale>
	//   <layerCount>
	//   per layer: <layerAssetPath>

	void LayeredMaterial::serialize(Archive& ar) const {
		ar << std::string(kTypeTag);
		ar << kFormatVersion;
		ar << name;
		ar << heightScale << macroStrength << macroScale;

		const uint32_t count = (uint32_t)std::min(layers.size(), (size_t)kMaxMaterialLayers);
		ar << count;

		for (uint32_t i = 0; i < count; ++i)
			ar << layerPath(layers[i]);
	}

	void LayeredMaterial::deserialize(Archive& ar) {
		std::string typeTag;
		ar >> typeTag;   // kTypeTag; ignored

		int32_t version = 0;
		ar >> version;
		if (version != kFormatVersion) {
			// Pre-layer-asset layout; discard rather than misread the body.
			VE_CORE_WARN_PRINT("LayeredMaterial: unsupported format version %d, layers cleared", version);
			return;
		}

		ar >> name;
		ar >> heightScale >> macroStrength >> macroScale;

		int32_t count = 0;
		ar >> count;
		count = std::clamp(count, 0, kMaxMaterialLayers);

		layers.clear();
		layers.resize((size_t)count);
		for (int32_t i = 0; i < count; ++i)
			layers[i] = loadLayer(ar);
	}

	void LayeredMaterial::writeNewAsset(const std::string& path) {
		const std::string filePath = withVeassetExt(path);

		auto material = CreateRef<LayeredMaterial>();
		material->name = toRelative(filePath);
		// No layers: the user assigns shared MaterialLayer assets from the terrain
		// inspector. A zero-layer material draws black until one is added.

		TextArchive ar(filePath, ArchiveMode::write);
		if (ar.isGood()) {
			material->serialize(ar);
		}
	}

}
