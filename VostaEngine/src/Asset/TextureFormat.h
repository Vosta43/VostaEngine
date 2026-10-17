#pragma once

namespace ve {

	enum TextureFormat {
		NONE = 0,
		RGB = 1,
		RGBA = 2,
		R8 = 3,
		RG16F = 4,
		RGB16F = 5,
		RGBA16F = 6,
		R16F = 7,
		// Color (sRGB-encoded) byte formats. Same storage as RGB / RGBA, but the
		// GPU decodes to linear on sample, so mip generation and filtering happen
		// in linear space. Appended after the original values so the int32 stored
		// in existing .veasset files keeps its old meaning (legacy = linear).
		SRGB8 = 8,
		SRGB8_ALPHA8 = 9,
		// Single-channel 16-bit unsigned integer; backs the RVT page table (each texel
		// is a tile-cache layer index). Appended so existing on-disk values keep theirs.
		R16UI = 10,
		// Single-channel 32-bit unsigned integer; backs the RVT feedback counter
		// image (one texel per pyramid page, bumped once per sampling fragment).
		R32UI = 11
	};

	// Number of logical channels a texture format carries. Single source of truth
	// for format metadata; consumers must query this instead of re-deriving it.
	inline int getChannelCount(TextureFormat format) {
		switch (format) {
			case TextureFormat::R8:
			case TextureFormat::R16F:
			case TextureFormat::R16UI:
			case TextureFormat::R32UI:       return 1;
			case TextureFormat::RG16F:       return 2;
			case TextureFormat::RGB:
			case TextureFormat::RGB16F:
			case TextureFormat::SRGB8:       return 3;
			case TextureFormat::RGBA:
			case TextureFormat::RGBA16F:
			case TextureFormat::SRGB8_ALPHA8: return 4;
			default:                         return 3;
		}
	}

	// The sRGB-encoded counterpart of an LDR byte format, or the input unchanged
	// for formats that have no color-space variants (data maps, float formats).
	inline TextureFormat srgbVariantOf(TextureFormat format) {
		switch (format) {
			case TextureFormat::RGB:  return TextureFormat::SRGB8;
			case TextureFormat::RGBA: return TextureFormat::SRGB8_ALPHA8;
			default:                  return format;
		}
	}

	// True for the formats the GPU decodes from sRGB to linear on sample.
	inline bool isSrgbFormat(TextureFormat format) {
		return format == TextureFormat::SRGB8 || format == TextureFormat::SRGB8_ALPHA8;
	}

}
