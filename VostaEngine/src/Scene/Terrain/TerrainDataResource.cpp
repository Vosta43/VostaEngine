#include "vepch.h"
#include "Scene/Terrain/TerrainDataResource.h"
#include "Scene/Archive.h"
#include "Core/Log.h"

#include <algorithm>

namespace ve {

	namespace {
		constexpr int32_t kTerrainDataFormatVersion = 1;

		bool tileHasData(const TerrainDataTile& t) {
			return std::any_of(t.weightData.begin(), t.weightData.end(),
				[](uint8_t b) { return b != 0; });
		}
	}

	TerrainDataTile& TerrainDataResource::tile(const glm::ivec2& c) {
		return tiles[pack(c)];
	}

	TerrainDataTile* TerrainDataResource::find(const glm::ivec2& c) {
		auto it = tiles.find(pack(c));
		return it == tiles.end() ? nullptr : &it->second;
	}

	void TerrainDataResource::erase(const glm::ivec2& c) {
		tiles.erase(pack(c));
	}

	bool TerrainDataResource::anyPainted() const {
		for (const auto& kv : tiles)
			if (!kv.second.weightData.empty() && tileHasData(kv.second))
				return true;
		return false;
	}

	void TerrainDataResource::write(const std::string& path) const {
		BinaryArchive ar(path, ArchiveMode::write);
		if (!ar.isGood()) {
			VE_CORE_ERROR_PRINT("TerrainDataResource: cannot open %s for write", path.c_str());
			return;
		}

		// Only tiles that actually carry paint are emitted, so an unpainted
		// terrain writes a header-only file rather than a full grid of zeros.
		std::vector<std::pair<int64_t, const TerrainDataTile*>> kept;
		for (const auto& kv : tiles)
			if (!kv.second.weightData.empty() && tileHasData(kv.second))
				kept.emplace_back(kv.first, &kv.second);

		ar << std::string("terrain_data");
		ar << kTerrainDataFormatVersion;
		ar << static_cast<int32_t>(kept.size());

		for (const auto& entry : kept) {
			const int64_t key = entry.first;
			const TerrainDataTile& t = *entry.second;
			ar << static_cast<int32_t>(static_cast<int32_t>(key >> 32));
			ar << static_cast<int32_t>(static_cast<uint32_t>(key & 0xffffffffu));
			ar << static_cast<int32_t>(t.weightRes);
			ar << static_cast<int32_t>(t.weightData.size());
			if (!t.weightData.empty())
				ar.writeBytes(t.weightData.data(), t.weightData.size());
		}

		ar.flush();
	}

	Ref<TerrainDataResource> TerrainDataResource::create(const std::string& path) {
		BinaryArchive ar(path, ArchiveMode::read);
		if (!ar.isGood()) {
			VE_CORE_WARN_PRINT("TerrainDataResource: cannot open %s", path.c_str());
			return nullptr;
		}

		std::string token;
		int32_t version = 0;
		ar >> token;
		ar >> version;
		if (token != "terrain_data" || version != kTerrainDataFormatVersion) {
			VE_CORE_WARN_PRINT("TerrainDataResource: bad header in %s", path.c_str());
			return nullptr;
		}

		auto resource = CreateRef<TerrainDataResource>();

		int32_t count = 0;
		ar >> count;
		for (int32_t i = 0; i < count; ++i) {
			int32_t cx = 0, cz = 0, weightRes = 0, bytes = 0;
			ar >> cx >> cz >> weightRes >> bytes;

			TerrainDataTile t;
			t.weightRes = weightRes;
			if (bytes > 0) {
				t.weightData.resize(static_cast<size_t>(bytes));
				ar.readBytes(t.weightData.data(), static_cast<size_t>(bytes));
			}
			resource->tiles[pack(glm::ivec2(cx, cz))] = std::move(t);
		}

		return resource;
	}

}
