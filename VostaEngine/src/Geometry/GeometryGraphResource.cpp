#include "vepch.h"
#include "Geometry/GeometryGraphResource.h"
#include "Scene/Archive.h"
#include "Core/Hash.h"
#include "Core/AssetConfig.h"
#include "Core/ResourceManager.h"
#include "Core/Log.h"

namespace ve {

	namespace {
		// Must match GeometryGraphPanel's on-disk version. A mismatch discards the file
		// rather than misreading it.
		constexpr int32_t kGeometryFormatVersion = 1;

		bool loadFromFile(const std::string& path, GeometryGraphResource& out) {
			const std::string file = toAbsolute(path);
			TextArchive ar(file, ArchiveMode::read);
			if (!ar.isGood()) {
				VE_CORE_WARN_PRINT("GeometryGraphResource: cannot open %s", file.c_str());
				return false;
			}

			std::string token;
			int32_t version = 0;
			ar >> token;
			ar >> version;
			if (token != "geometry" || version != kGeometryFormatVersion) {
				VE_CORE_WARN_PRINT("GeometryGraphResource: bad header in %s", file.c_str());
				return false;
			}

			deserializeGeometryGraph(out.graph, ar);
			out.contentHash = hashFileBytes(file);
			return true;
		}
	}

	Ref<GeometryGraphResource> GeometryGraphResource::create(const std::string& path) {
		auto resource = CreateRef<GeometryGraphResource>();
		if (!loadFromFile(path, *resource))
			return nullptr;
		return resource;
	}

	void GeometryGraphResource::refreshFromFile(const std::string& path) {
		const AssetHandle handle = ResourceManager::find<GeometryGraphResource>(path);
		if (!handle.isValid())
			return;
		if (Ref<GeometryGraphResource> resource = ResourceManager::get<GeometryGraphResource>(handle))
			loadFromFile(path, *resource);
	}

}
