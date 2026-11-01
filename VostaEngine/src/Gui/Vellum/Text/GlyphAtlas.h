#pragma once

#include "Core/Core.h"
#include "Gui/Vellum/Core/DrawList.h"   // TextureId

#include <glm.hpp>
#include <cstdint>
#include <unordered_map>
#include <vector>

namespace ve {
	class Texture2D;
}

namespace ve::vellum {

	class VellumRenderer;

	// The renderer's one glyph atlas: every font at every pixel size rasterises
	// into this single texture. A screen mixing sizes then still batches as one
	// draw, and adding a size costs only the glyphs it actually shows.
	//
	// A glyph is keyed by (font id, pixel size, codepoint). The atlas grows by
	// doubling its height and keeps its width, so U coordinates survive growth;
	// cached V coordinates are rescale-normalised in grow(). It owns the texture
	// and registers it with the renderer, which must outlive it.
	class VE_API GlyphAtlas {
	public:
		// One rasterised glyph, in pixels. `bearing` is relative to the glyph
		// origin on the baseline; y grows downward, so it is negative above the
		// baseline.
		struct Glyph {
			glm::vec2 uv0{ 0.0f };
			glm::vec2 uv1{ 0.0f };
			glm::vec2 size{ 0.0f };
			glm::vec2 bearing{ 0.0f };
			float     advance = 0.0f;
		};

		struct Key {
			uint64_t  fontId = 0;
			float     size = 0.0f;
			uint32_t  codepoint = 0;

			bool operator==(const Key& other) const {
				return fontId == other.fontId && size == other.size
					&& codepoint == other.codepoint;
			}
		};

		explicit GlyphAtlas(VellumRenderer& renderer);

		TextureId textureId() const { return m_id; }

		const Glyph* find(const Key& key) const;

		// Packs `coverage` (w x h, 8-bit) and returns the stored entry. A null or
		// empty bitmap still stores `advance` with a zero-size quad, so layout flows
		// for codepoints no face has an outline for.
		const Glyph* insert(const Key& key, uint32_t w, uint32_t h,
		                    const unsigned char* coverage, glm::vec2 bearing, float advance);

	private:
		struct KeyHash {
			std::size_t operator()(const Key& key) const;
		};

		// Places one glyph on the current shelf; returns false when it is wider
		// than the atlas or growth is capped.
		bool place(uint32_t w, uint32_t h, const unsigned char* coverage,
		           glm::vec2& uv0, glm::vec2& uv1);

		// Doubles the height until `requiredHeight` fits, re-uploads, and rescales
		// every cached V coordinate.
		void grow(uint32_t requiredHeight);

		VellumRenderer* m_renderer = nullptr;
		TextureId       m_id = 0;
		Ref<Texture2D>  m_texture;

		std::vector<unsigned char> m_pixels;   // RGBA slab, top-left origin
		uint32_t m_width = 0;
		uint32_t m_height = 0;
		uint32_t m_cursorX = 0;
		uint32_t m_cursorY = 0;
		uint32_t m_rowHeight = 0;

		std::unordered_map<Key, Glyph, KeyHash> m_glyphs;
	};

}
