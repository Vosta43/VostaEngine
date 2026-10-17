#include "vepch.h"
#include "MaterialLayerAsset.h"

#include "Core/Log.h"
#include "Core/ResourceManager.h"
#include "Core/AssetConfig.h"
#include "Renderer/Material.h"
#include "Renderer/Texture.h"

namespace ve {

	namespace {

		std::string withVeassetExt(const std::string& path) {
			if (path.size() < 8 || path.compare(path.size() - 8, 8, ".veasset") != 0)
				return path + ".veasset";
			return path;
		}

		std::string texPath(AssetHandle h) {
			return h.isValid() ? ResourceManager::getPath<Texture2D>(h) : std::string();
		}

		std::string matPath(AssetHandle h) {
			return h.isValid() ? ResourceManager::getPath<Material>(h) : std::string();
		}

		AssetHandle loadTex(Archive& ar) {
			std::string p;
			ar >> p;
			return p.empty() ? INVALID_ASSET_HANDLE : ResourceManager::store<Texture2D>(p);
		}

		AssetHandle loadMat(Archive& ar) {
			std::string p;
			ar >> p;
			return p.empty() ? INVALID_ASSET_HANDLE : ResourceManager::store<Material>(p);
		}

	} // namespace

	Ref<MaterialLayerAsset> MaterialLayerAsset::create(const std::string& path) {
		TextArchive ar(path, ArchiveMode::read);
		if (!ar.isGood()) {
			VE_CORE_WARN_PRINT("MaterialLayerAsset: cannot open %s", path.c_str());
			return nullptr;
		}

		std::string token;
		int32_t version = 0;
		ar >> token;
		ar >> version;
		if (token != kTypeTag || version != kFormatVersion) {
			VE_CORE_WARN_PRINT("MaterialLayerAsset: bad header in %s", path.c_str());
			return nullptr;
		}

		auto resource = CreateRef<MaterialLayerAsset>();
		resource->deserialize(ar);
		return resource;
	}

	// --- Serialization (.veasset) ---
	//   "material_layer" <version>
	//   <name> <materialPath> <heightPath> <tiling>
	//   <useSlope> <slopeMin> <slopeMax>
	//   <useHeight> <heightMin> <heightMax>
	//   <noiseStrength> <noiseScale> <blend>

	void MaterialLayerAsset::serialize(Archive& ar) const {
		ar << std::string(kTypeTag);
		ar << kFormatVersion;

		ar << layer.name;
		ar << matPath(layer.material);
		ar << texPath(layer.heightMap);
		ar << layer.tiling;

		ar << (int32_t)(layer.useSlope ? 1 : 0);
		ar << layer.slopeMin << layer.slopeMax;
		ar << (int32_t)(layer.useHeight ? 1 : 0);
		ar << layer.heightMin << layer.heightMax;

		ar << layer.noiseStrength << layer.noiseScale;
		ar << (int32_t)layer.blend;
	}

	void MaterialLayerAsset::deserialize(Archive& ar) {
		ar >> layer.name;
		layer.material  = loadMat(ar);
		layer.heightMap = loadTex(ar);
		ar >> layer.tiling;

		int32_t flag = 0;
		ar >> flag; layer.useSlope = (flag != 0);
		ar >> layer.slopeMin >> layer.slopeMax;
		ar >> flag; layer.useHeight = (flag != 0);
		ar >> layer.heightMin >> layer.heightMax;

		ar >> layer.noiseStrength >> layer.noiseScale;
		ar >> flag; layer.blend = (MaterialLayerBlend)flag;
	}

	void MaterialLayerAsset::writeNewAsset(const std::string& path) {
		const std::string filePath = withVeassetExt(path);

		auto asset = CreateRef<MaterialLayerAsset>();
		asset->layer.name = "Layer";

		TextArchive ar(filePath, ArchiveMode::write);
		if (ar.isGood()) {
			asset->serialize(ar);
		}
	}

}
