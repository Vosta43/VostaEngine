#pragma once

#include "StaticMeshResource.h"

#include <string>

namespace ve {

	class VE_API StaticMeshImporter {
	public:
		
		static Ref<StaticMeshResource> importFromFile(const std::string& filePath);

	private:
		
	};
}