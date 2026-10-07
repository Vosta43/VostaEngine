#pragma once

#include "StaticMeshResource.h"

#include <string>

namespace ve {

	class VE_API StaticMeshImporter {
	public:

		static Ref<StaticMeshResource> importFromFile(const std::string& filePath);

		// Baked-mesh asset IO (.veasset, binary Archive). saveToAsset is what
		// import writes so later loads skip re-parsing the source .obj.
		static bool saveToAsset(const Ref<StaticMeshResource>& resource, const std::string& filePath);
		static Ref<StaticMeshResource> loadFromAsset(const std::string& filePath);

	private:

	};
}