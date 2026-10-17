#include "vepch.h"
#include "LayeredMaterial.h"
#include "Asset/BuiltinReousrces.h"
#include "Core/AssetConfig.h"
#include "Core/Log.h"
#include "Core/ResourceManager.h"
#include "Renderer/LayeredShaderBuilder.h"
#include "Renderer/MaterialLayerAsset.h"
#include "Renderer/Material.h"
#include "Renderer/Shader.h"
#include "Renderer/SingleMaterial.h"

#include <algorithm>

namespace ve {

	namespace {

		// Staged outside the resource registry: a consumer samples the baked pages, so
		// it never needs to know which material produced them.
		constexpr const char* kRVTShaderPath = "VostaEngine/resources/shaders/terrain_rvt.glsl";

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

		std::string handlePart(AssetHandle h) {
			return h.isValid()
				? std::to_string(h.index()) + "." + std::to_string(h.generation())
				: std::string("-");
		}

	} // namespace

	std::string LayeredMaterial::structureKey() const {
		std::string k = "v1";
		const int count = (int)std::min(layers.size(), (size_t)kMaxMaterialLayers);
		k += "|c" + std::to_string(count);

		for (int i = 0; i < count; ++i) {
			k += "|L" + handlePart(layers[i]);
			auto la = ResourceManager::get<MaterialLayerAsset>(layers[i]);
			if (!la) { k += "|none"; continue; }

			const MaterialLayer& L = la->layer;
			k += "|m" + handlePart(L.material) + "|h" + handlePart(L.heightMap);

			// A graph material's generated code changes with its revision; a fixed-slot
			// material's samplers change with the map handles it has assigned.
			auto sm = std::dynamic_pointer_cast<SingleMaterial>(
				ResourceManager::get<Material>(L.material));
			k += "|r" + std::to_string(sm ? sm->revision() : 0u);
			k += (sm && sm->graph.nodes.size() > 1) ? "|g1" : "|g0";
		}
		return k;
	}

	Ref<Shader> LayeredMaterial::getShader() {
		const std::string key = structureKey();
		// Returning early when the key is unchanged also covers the failed-build case:
		// m_shader stays null, so the pass keeps its default shader without rebuilding
		// (and re-warning) every frame.
		if (key == m_shaderKey)
			return m_shader;

		m_shaderKey = key;
		m_shader = nullptr;
		m_cachedTextures.clear();

		LayeredBuildResult res = LayeredShaderBuilder::build(*this);
		if (!res.ok) {
			VE_CORE_WARN_PRINT("LayeredMaterial '%s': shader build failed: %s",
			                   name.c_str(), res.error.c_str());
			return nullptr;
		}

		Ref<Shader> shader = Shader::create(name, res.vertSrc, res.fragSrc);
		if (!shader || !shader->isValid()) {
			VE_CORE_WARN_PRINT("LayeredMaterial '%s': generated shader failed to compile/link",
			                   name.c_str());
			return nullptr;
		}

		m_shader = shader;
		m_cachedTextures = std::move(res.textures);
		return m_shader;
	}

	Ref<Shader> LayeredMaterial::getRVTShader() {
		if (!sampleRVT) return nullptr;
		if (m_rvtShader) return m_rvtShader;

		m_rvtShader = Shader::create(toAbsolute(kRVTShaderPath));
		if (!m_rvtShader || !m_rvtShader->isValid()) {
			VE_CORE_WARN_PRINT("LayeredMaterial '%s': RVT consumer shader '%s' failed to load",
			                   name.c_str(), kRVTShaderPath);
			m_rvtShader = nullptr;
			return nullptr;
		}
		return m_rvtShader;
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
		// call so the raw MaterialLayer references below stay valid.
		std::vector<Ref<MaterialLayerAsset>> resolved(count);
		for (int i = 0; i < count; ++i)
			resolved[i] = ResourceManager::get<MaterialLayerAsset>(layers[i]);

		// Empty slots still get their scalars so the shader reads defaults rather
		// than whatever a previous draw left bound.
		static const MaterialLayer kDefaultLayer;

		for (int i = 0; i < count; ++i) {
			const MaterialLayer& layer = resolved[i] ? resolved[i]->layer : kDefaultLayer;
			out.floats.push_back({indexed("u_LayerTiling", i), layer.tiling});
			out.ints.push_back({indexed("u_LayerBlend", i), (int)layer.blend});
		}

		out.floats.push_back({"u_MacroStrength", macroStrength});
		out.floats.push_back({"u_MacroScale", macroScale});

		// Globe coordinate mode's tiling (only read when the pass sets u_CoordMode 1);
		// harmless on the chart path.
		out.floats.push_back({"u_GlobeTiles", LayeredShaderBuilder::kGlobeTiles});
		out.floats.push_back({"u_GlobeMacroTiles", LayeredShaderBuilder::kGlobeMacroTiles});

		// Per-terrain weight array, declared here so a MaterialInstance can override it
		// by name. One R8 slice per layer; the default is all-zero with invSize 0, so an
		// untargeted surface shows only the base layer. The unit sits at the top of the
		// budget so it never collides with a generated content sampler.
		out.textures.push_back({"u_Weights", BuiltinResources::getDefaultWeightArray(),
		                        LayeredShaderBuilder::kWeightsTextureUnit,
		                        TextureTarget::Texture2DArray});
		out.vec2s.push_back({"u_TerrainOrigin", glm::vec2(0.0f)});
		out.floats.push_back({"u_TerrainInvSize", 0.0f});

		// The generated shader's own samplers, in the units the builder assigned.
		for (const auto& t : m_cachedTextures)
			out.textures.push_back(t);
	}

	// --- Serialization (.veasset) ---
	//   "layered_material" <version>
	//   <name> <heightScale> <macroStrength> <macroScale>
	//   <layerCount>
	//   per layer: <layerAssetPath>
	//   <sampleRVT>

	void LayeredMaterial::serialize(Archive& ar) const {
		ar << std::string(kTypeTag);
		ar << kFormatVersion;
		ar << name;
		ar << heightScale << macroStrength << macroScale;

		const uint32_t count = (uint32_t)std::min(layers.size(), (size_t)kMaxMaterialLayers);
		ar << count;

		for (uint32_t i = 0; i < count; ++i)
			ar << layerPath(layers[i]);

		// Appended after the layer list instead of folded into a new body version: a
		// file written before the flag existed simply runs out of tokens and reads
		// back false, so this costs no migration.
		ar << (int32_t)(sampleRVT ? 1 : 0);
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

		// Absent in files written before the RVT path existed; a failed read leaves it
		// at its default.
		int32_t rvt = 0;
		ar >> rvt;
		sampleRVT = rvt != 0;

		// The body changed on disk: drop the generated shaders so they rebuild on next
		// draw, and mark the revision so any holder keying on it sees the change.
		m_shader = nullptr;
		m_shaderKey.clear();
		m_cachedTextures.clear();
		m_rvtShader = nullptr;
		bumpRevision();
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
