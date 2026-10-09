#pragma once

#include "StaticMeshResource.h"

#include <string>
#include <vector>

namespace ve {

	class VE_API StaticMeshImporter {
	public:

		static Ref<StaticMeshResource> importFromFile(const std::string& filePath);

		// External texture files referenced by a mesh source, as bare filenames.
		// ImportManager copies these next to the baked asset so the mesh stays
		// self-contained. Empty for formats with no external references.
		static std::vector<std::string> referencedTextures(const std::string& filePath);

		// Baked-mesh asset IO (.veasset, binary Archive). saveToAsset is what
		// import writes so later loads skip re-parsing the source .obj.
		static bool saveToAsset(const Ref<StaticMeshResource>& resource, const std::string& filePath);
		static Ref<StaticMeshResource> loadFromAsset(const std::string& filePath);

	private:

	};
}