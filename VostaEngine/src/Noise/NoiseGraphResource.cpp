#include "vepch.h"
#include "Noise/NoiseGraphResource.h"
#include "Scene/Archive.h"
#include "Core/Hash.h"
#include "Core/AssetConfig.h"
#include "Core/ResourceManager.h"
#include "Core/Log.h"

namespace ve {

	namespace {
		// Must match NoisePanel's on-disk version. A mismatch discards the file rather
		// than misreading it.
		constexpr int32_t kNoiseFormatVersion = 2;

		bool loadFromFile(const std::string& path, NoiseGraphResource& out) {
			const std::string file = toAbsolute(path);
			TextArchive ar(file, ArchiveMode::read);
			if (!ar.isGood()) {
				VE_CORE_WARN_PRINT("NoiseGraphResource: cannot open %s", file.c_str());
				return false;
			}

			std::string token;
			int32_t version = 0;
			ar >> token;
			ar >> version;
			if (token != "noise" || version != kNoiseFormatVersion) {
				VE_CORE_WARN_PRINT("NoiseGraphResource: bad header in %s", file.c_str());
				return false;
			}

			deserializeNoiseGraph(out.graph, ar);
			out.contentHash = hashFileBytes(file);
			return true;
		}
	}

	Ref<NoiseGraphResource> NoiseGraphResource::create(const std::string& path) {
		auto resource = CreateRef<NoiseGraphResource>();
		if (!loadFromFile(path, *resource))
			return nullptr;
		return resource;
	}

	void NoiseGraphResource::refreshFromFile(const std::string& path) {
		const AssetHandle handle = ResourceManager::find<NoiseGraphResource>(path);
		if (!handle.isValid())
			return;
		if (Ref<NoiseGraphResource> resource = ResourceManager::get<NoiseGraphResource>(handle))
			loadFromFile(path, *resource);
	}

}
