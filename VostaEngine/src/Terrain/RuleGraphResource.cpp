#include "vepch.h"
#include "Terrain/RuleGraphResource.h"
#include "Scene/Archive.h"
#include "Core/Hash.h"
#include "Core/AssetConfig.h"
#include "Core/ResourceManager.h"
#include "Core/Log.h"

namespace ve {

	namespace {
		// Must match TerrainRulePanel's on-disk version. A mismatch discards the file
		// rather than misreading it.
		constexpr int32_t kRuleFormatVersion = 2;

		bool loadFromFile(const std::string& path, TerrainRuleGraphResource& out) {
			const std::string file = toAbsolute(path);
			TextArchive ar(file, ArchiveMode::read);
			if (!ar.isGood()) {
				VE_CORE_WARN_PRINT("TerrainRuleGraphResource: cannot open %s", file.c_str());
				return false;
			}

			std::string token;
			int32_t version = 0;
			ar >> token;
			ar >> version;
			if (token != "terrain_rule" || version != kRuleFormatVersion) {
				VE_CORE_WARN_PRINT("TerrainRuleGraphResource: bad header in %s", file.c_str());
				return false;
			}

			deserializeRuleGraph(out.graph, ar);
			out.contentHash = hashFileBytes(file);
			return true;
		}
	}

	Ref<TerrainRuleGraphResource> TerrainRuleGraphResource::create(const std::string& path) {
		auto resource = CreateRef<TerrainRuleGraphResource>();
		if (!loadFromFile(path, *resource))
			return nullptr;
		return resource;
	}

	void TerrainRuleGraphResource::refreshFromFile(const std::string& path) {
		const AssetHandle handle = ResourceManager::find<TerrainRuleGraphResource>(path);
		if (!handle.isValid())
			return;
		if (Ref<TerrainRuleGraphResource> resource = ResourceManager::get<TerrainRuleGraphResource>(handle))
			loadFromFile(path, *resource);
	}

}
