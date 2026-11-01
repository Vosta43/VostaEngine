#pragma once

#include "Core/Core.h"
#include "Gui/Vellum/Core/Widget.h"

#include <glm.hpp>
#include <string>

namespace ve::vellum {

	class Font;
	class FontAtlas;

	// A single line of UTF-8 text drawn with a FontAtlas. The font is borrowed, not
	// owned, and is non-const because a first-time codepoint rasterises into it on
	// demand. One line only — no wrapping, no newlines.
	class VE_API TextBlock : public Widget {
	public:
		explicit TextBlock(FontAtlas* font,
		                   const std::string& text,
		                   const glm::vec4& color = glm::vec4(1.0f));

		// Text sized in logical units whose glyphs are rasterised at the size they
		// actually occupy on screen: the atlas is rebuilt at `logicalPixelSize *
		// layoutScale()` while the block itself keeps its logical bounds. `font` is
		// the face chain, not one size of it.
		TextBlock(Font* font, float logicalPixelSize,
		          const std::string& text,
		          const glm::vec4& color = glm::vec4(1.0f));

		// Swaps the borrowed atlas (the font is chosen by pixel size at build time).
		void setFont(FontAtlas* font);

		void setText(const std::string& text);
		const std::string& text() const { return m_text; }
		void setColor(const glm::vec4& color) { m_color = color; }

		void arrange(const Rect& slot) override;
		void paint(PaintContext& ctx) override;

	protected:
		glm::vec2 computeDesiredSize() const override;

	private:
		// The atlas this frame's text is measured and drawn with.
		FontAtlas* activeAtlas() const;
		// Logical units per atlas pixel (1 unless the block resolves its own size).
		float inverseScale() const;

		FontAtlas* m_font = nullptr;
		Font*      m_fontChain = nullptr;   // set only for the scale-aware ctor
		float      m_logicalSize = 0.0f;    // logical pixels, scale-aware ctor only
		std::string m_text;
		glm::vec4 m_color;
	};

}
