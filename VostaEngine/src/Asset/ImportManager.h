#pragma once

#include "Utils.h"
#include <string>

namespace ve {
	
	class VE_API ImportManager {
	public:
		// Import sourcePath into destDir. The source and everything it
		// references (OBJ -> MTL -> textures) is copied into destDir first, so
		// the project stays self-contained; a mesh then gets baked to a
		// <stem>.veasset next to it.
		static void import(const std::string& sourcePath, const std::string& destDir);

	private:


	};

}