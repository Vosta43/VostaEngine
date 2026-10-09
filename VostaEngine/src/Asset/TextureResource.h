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

	// Number of logical channels a texture format carries. Single source of truth
	// for format metadata; consumers must query this instead of re-deriving it.
	inline int getChannelCount(TextureFormat format) {
		switch (format) {
			case TextureFormat::R8:
			case TextureFormat::R16F:    return 1;
			case TextureFormat::RG16F:   return 2;
			case TextureFormat::RGB:
			case TextureFormat::RGB16F:  return 3;
			case TextureFormat::RGBA:
			case TextureFormat::RGBA16F: return 4;
			default:                     return 3;
		}
	}

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
