#pragma once

#include "Core/Core.h"
#include "TextureResource.h"

#include <string>

namespace ve {
	
	class VE_API TextureImporter {
	public:
		// Import and return as A ENGINE RESOURCE
		static Ref<TextureResource> importFromFile(const std::string& filePath);
		// Save engine asset (NOT RAW FILE!)
		static void serialize(const TextureResource& resource, const std::string& serializePath);
		// Load engine asset (NOT RAW FILE!)
		static Ref<TextureResource> deserialize(const std::string& serializePath);

	private:
		

	};

	class TextureCubeMapImporter {
	public:

		static Ref<TextureCubeMapResource> importFromFile(const std::string& filePath);
		// Save engine asset (NOT RAW FILE!)
		static void serialize(const TextureCubeMapResource& resource, const std::string& serializePath);
		// Load engine asset (NOT RAW FILE!)
		static Ref<TextureCubeMapResource> deserialize(const std::string& serializePath);
		
	};

}