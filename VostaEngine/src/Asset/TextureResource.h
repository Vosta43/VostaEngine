#pragma once

#include <cstdint>
#include <vector>
#include <array>

namespace ve {

	enum TextureFormat {
		NONE = 0,
		RGB = 1,
		RGBA = 2,
		R8 = 3,
		RG16F = 4,
		RGB16F = 5,
		RGBA16F = 6,
		R16F = 7
	};

	struct TextureResource {

		std::string sourceFilePath;

		uint32_t width;
		uint32_t height;

		std::vector<unsigned char> pixels;
		std::vector<float> floatPixels;
		TextureFormat format = TextureFormat::RGB;
	};

	struct TextureCubeMapResource {

		// Source file is ONE IMAGE that will be cut into 6 pieces during import
		std::string sourceFilePathPath;
		uint32_t width;
		uint32_t height;

		std::array<std::vector<float>, 6> facePixels;
		TextureFormat format = TextureFormat::RGB;
	};

}
