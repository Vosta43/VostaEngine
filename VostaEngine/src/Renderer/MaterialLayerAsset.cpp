#include "vepch.h"
#include "MaterialLayerAsset.h"

#include "Core/Log.h"
#include "Core/ResourceManager.h"
#include "Core/AssetConfig.h"
#include "Asset/Utils.h"
#include "Asset/TextureImporter.h"
#include "Asset/TextureResource.h"
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

		AssetHandle loadTex(Archive& ar) {
			std::string p;
			ar >> p;
			return p.empty() ? INVALID_ASSET_HANDLE : ResourceManager::store<Texture2D>(p);
		}

		// A decoded, CPU-side view of a source texture. Owns the resource so `px`
		// stays valid for as long as the view lives. Holds LDR byte pixels only
		// (RGB / RGBA / R8) — float sources are not packed.
		struct Src {
			Ref<TextureResource> owner;
			const unsigned char* px = nullptr;
			uint32_t w = 0, h = 0;
			int ch = 0;
			bool valid() const { return px != nullptr; }
		};

		Src makeSrc(const Ref<TextureResource>& tr) {
			Src s;
			if (!tr || tr->pixels.empty() || tr->width == 0 || tr->height == 0)
				return s;
			switch (tr->format) {
			case TextureFormat::RGBA: s.ch = 4; break;
			case TextureFormat::RGB:  s.ch = 3; break;
			case TextureFormat::R8:   s.ch = 1; break;
			default: return s;
			}
			s.owner = tr;
			s.px = s.owner->pixels.data();
			s.w = s.owner->width;
			s.h = s.owner->height;
			return s;
		}

		// Load a source texture's CPU pixels the same way Texture2D::create does:
		// baked .veasset through the importer's binary reader, a raw image through
		// the file importer.
		Ref<TextureResource> loadSource(const AssetHandle& h) {
			if (!h.isValid())
				return nullptr;
			const std::string& rel = ResourceManager::getPath<Texture2D>(h);
			if (rel.empty())
				return nullptr;
			const std::string abs = toAbsolute(rel);
			return utils::isEngineAsset(abs) ? TextureImporter::deserialize(abs)
			                                 : TextureImporter::importFromFile(abs);
		}

		// Nearest-sample channel `c` of `s` at target pixel (x,y) of a tw x th image.
		unsigned char tap(const Src& s, uint32_t x, uint32_t y, uint32_t tw, uint32_t th, int c) {
			uint32_t sx = (uint32_t)((uint64_t)x * s.w / tw);
			uint32_t sy = (uint32_t)((uint64_t)y * s.h / th);
			if (sx >= s.w) sx = s.w - 1;
			if (sy >= s.h) sy = s.h - 1;
			const int cc = (c < s.ch) ? c : s.ch - 1;   // grayscale replicates to rgb
			return s.px[((size_t)sy * s.w + sx) * s.ch + cc];
		}

		// Pack one RGBA image: rgb from `rgbSrc` (or the fallback colour), alpha from
		// `aSrc`'s red channel (or `aDefault`). Size comes from whichever source is
		// present; the other is nearest-sampled to match.
		Ref<TextureResource> pack(const Src& rgbSrc, const Src& aSrc,
		                          uint32_t fbR, uint32_t fbG, uint32_t fbB, uint32_t aDefault) {
			uint32_t w = 1, h = 1;
			if (rgbSrc.valid()) { w = rgbSrc.w; h = rgbSrc.h; }
			else if (aSrc.valid()) { w = aSrc.w; h = aSrc.h; }

			auto tr = CreateRef<TextureResource>();
			tr->width = w;
			tr->height = h;
			tr->format = TextureFormat::RGBA;
			tr->pixels.resize((size_t)w * h * 4);

			for (uint32_t y = 0; y < h; ++y) {
				for (uint32_t x = 0; x < w; ++x) {
					const size_t o = ((size_t)y * w + x) * 4;
					if (rgbSrc.valid()) {
						tr->pixels[o + 0] = tap(rgbSrc, x, y, w, h, 0);
						tr->pixels[o + 1] = tap(rgbSrc, x, y, w, h, 1);
						tr->pixels[o + 2] = tap(rgbSrc, x, y, w, h, 2);
					}
					else {
						tr->pixels[o + 0] = (unsigned char)fbR;
						tr->pixels[o + 1] = (unsigned char)fbG;
						tr->pixels[o + 2] = (unsigned char)fbB;
					}
					tr->pixels[o + 3] = aSrc.valid() ? tap(aSrc, x, y, w, h, 0) : (unsigned char)aDefault;
				}
			}
			return tr;
		}

		// The key embeds the owning asset as well as the four source handles: two
		// layers that happen to reference the same maps must still get their own
		// textures, or the second pack would evict the first asset's registry slot
		// and leave it holding a dangling handle.
		std::string packKey(const MaterialLayerAsset* owner, const MaterialLayer& l) {
			auto part = [](AssetHandle h) {
				return h.isValid()
					? std::to_string(h.index()) + "." + std::to_string(h.generation())
					: std::string("-");
			};
			return "__layer_pack_" + std::to_string((uintptr_t)owner) + "_" +
				part(l.albedoMap) + "_" + part(l.normalMap) + "_" +
				part(l.roughnessMap) + "_" + part(l.heightMap);
		}

		AssetHandle registerPacked(const std::string& key, const Ref<TextureResource>& tr) {
			auto tex = Texture2D::create(tr);
			if (!tex)
				return INVALID_ASSET_HANDLE;
			auto& storage = ResourceManager::getStorage<Texture2D>();
			storage.remove(key);
			return storage.store(key, tex);
		}

	} // namespace

	const MaterialLayerAsset::PackedTextures& MaterialLayerAsset::packedTextures() const {
		const std::array<AssetHandle, 4> key = {
			layer.albedoMap, layer.normalMap, layer.roughnessMap, layer.heightMap
		};
		if (m_packValid && key == m_packKey)
			return m_packed;

		// Release the previous pack before building the new one. The key covers the
		// pointer and the handles, so a repack yields a different key; without this
		// the old slots would stay resident and bound to the same asset forever.
		auto& texStorage = ResourceManager::getStorage<Texture2D>();
		if (m_packed.albedoHeight.isValid())
			texStorage.remove(m_packed.albedoHeight);
		if (m_packed.normalRoughness.isValid())
			texStorage.remove(m_packed.normalRoughness);
		m_packed = PackedTextures{};
		m_packValid = false;

		const Src albedo = makeSrc(loadSource(layer.albedoMap));
		const Src normal = makeSrc(loadSource(layer.normalMap));
		const Src rough  = makeSrc(loadSource(layer.roughnessMap));
		const Src height = makeSrc(loadSource(layer.heightMap));

		const std::string base = packKey(this, layer);
		// albedo rgb + height a; missing height -> 1.0 so the layer is fully present.
		m_packed.albedoHeight = registerPacked(base + "#ah",
			pack(albedo, height, 255, 255, 255, 255));
		// normal rgb (flat when missing) + roughness a (1.0 when missing).
		m_packed.normalRoughness = registerPacked(base + "#nr",
			pack(normal, rough, 128, 128, 255, 255));

		m_packKey = key;
		m_packValid = true;
		return m_packed;
	}

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
	//   <name> <albedoPath> <normalPath> <roughnessPath> <heightPath> <tiling>
	//   <useSlope> <slopeMin> <slopeMax>
	//   <useHeight> <heightMin> <heightMax>
	//   <noiseStrength> <noiseScale> <blend>

	void MaterialLayerAsset::serialize(Archive& ar) const {
		ar << std::string(kTypeTag);
		ar << kFormatVersion;

		ar << layer.name;
		ar << texPath(layer.albedoMap);
		ar << texPath(layer.normalMap);
		ar << texPath(layer.roughnessMap);
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
		layer.albedoMap = loadTex(ar);
		layer.normalMap = loadTex(ar);
		layer.roughnessMap = loadTex(ar);
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
