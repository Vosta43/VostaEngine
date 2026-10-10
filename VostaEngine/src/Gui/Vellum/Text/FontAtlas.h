#pragma once

#include "Core/Core.h"
#include "Gui/Vellum/Core/DrawList.h"

#include <glm.hpp>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

struct stbtt_fontinfo;   // global tag, matches stb_truetype.h

namespace ve {
	class Texture2D;
}

namespace ve::vellum {

	class VellumRenderer;

	// A grow-only dynamic glyph atlas: one face at one pixel size, rasterised on
	// demand. RGB is white and alpha is coverage, so the UI shader's
	// `v_Color * texture(...)` tints text with no special case.
	//
	// A cache miss rasterises the glyph, packs it onto the current shelf, and
	// uploads just that block; running out of height doubles the texture and
	// re-uploads. Both need a live GL context and the renderer to re-register
	// with, so instances are created through FontManager rather than directly.
	class VE_API FontAtlas {
	public:
		~FontAtlas();

		// Reads `ttfPath` and prepares an empty atlas for `pixelSize`. Glyphs are
		// rasterised lazily. `renderer` must outlive the atlas; `id` is the logical
		// texture handle it registers under. Returns null on an unreadable face.
		static Ref<FontAtlas> create(const std::string& ttfPath, float pixelSize,
		                             TextureId id, VellumRenderer& renderer);

		TextureId textureId() const { return m_id; }

		// One rasterised glyph, in pixels. `bearing` is relative to the glyph origin
		// on the baseline; y grows downward, so it is negative above the baseline.
		struct Glyph {
			glm::vec2 uv0{ 0.0f };
			glm::vec2 uv1{ 0.0f };
			glm::vec2 size{ 0.0f };
			glm::vec2 bearing{ 0.0f };
			float     advance = 0.0f;
		};

		// Looks `codepoint` up, rasterising and inserting it on a miss. Returns null
		// only when the glyph cannot be placed (wider than the atlas). Codepoints the
		// face has no outline for come back with a zero-size quad and a real advance,
		// so layout still flows.
		const Glyph* get(uint32_t codepoint);

		// One-line width and line box of `text`, decoded as UTF-8.
		glm::vec2 measure(const std::string& text);
		float ascent() const { return m_ascent; }
		float lineHeight() const { return m_lineHeight; }

	private:
		// Places one glyph: wraps to a new shelf row if needed, grows the atlas if
		// the row falls off the bottom, writes coverage into the slab, and uploads
		// the block. Returns false when the glyph is wider than the whole atlas.
		bool insert(uint32_t w, uint32_t h, const unsigned char* bitmap,
		            glm::vec2& uv0, glm::vec2& uv1);

		// Doubles the height (once or repeatedly) until `requiredHeight` fits, then
		// re-creates the texture and re-registers it under the same id.
		void grow(uint32_t requiredHeight);

		VellumRenderer* m_renderer = nullptr;
		TextureId       m_id = 0;

		std::vector<unsigned char> m_ttf;      // backing store stb points into
		stbtt_fontinfo* m_info = nullptr;
		float m_scale = 0.0f;

		std::vector<unsigned char> m_pixels;   // RGBA slab, top-left origin
		uint32_t m_width = 0;
		uint32_t m_height = 0;
		uint32_t m_cursorX = 0;
		uint32_t m_cursorY = 0;
		uint32_t m_rowHeight = 0;
		Ref<Texture2D> m_texture;

		std::unordered_map<uint32_t, Glyph> m_glyphs;

		float m_ascent = 0.0f;
		float m_lineHeight = 0.0f;
	};

}
