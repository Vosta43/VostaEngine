#pragma once

#include "Core/Core.h"
#include "Gui/Vellum/Core/Widget.h"

#include <glm.hpp>
#include <string>
#include <vector>

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
		void setColor(const glm::vec4& color) { m_color = color; clearGradient(); }

		// One colour stop on a gradient ramp, at a position in 0..1 along the block.
		struct GradientStop {
			float     position = 0.0f;
			glm::vec4 color{ 1.0f };
		};

		enum class GradientAxis { Horizontal, Vertical };

		// Replaces the flat colour with a ramp sampled across the block instead of a
		// per-glyph colour, so tall and short glyphs share one continuous ramp.
		// Stops must ascend by position. `cyclesPerSecond` scrolls the ramp and is
		// wrapped, so a stop list whose last colour matches its first animates
		// seamlessly; 0 paints a static gradient. setColor() clears the ramp.
		void setGradient(std::vector<GradientStop> stops,
		                 GradientAxis axis = GradientAxis::Horizontal,
		                 float cyclesPerSecond = 0.0f);
		void clearGradient() { m_gradientStops.clear(); }

		void tick(float dt) override;

		void arrange(const Rect& slot) override;
		void onPaint(PaintContext& ctx) override;

	protected:
		glm::vec2 computeDesiredSize() const override;

	private:
		// The atlas this frame's text is measured and drawn with.
		FontAtlas* activeAtlas() const;
		// Logical units per atlas pixel (1 unless the block resolves its own size).
		float inverseScale() const;
		// Samples the ramp at `t`, after scrolling and wrapping to 0..1.
		glm::vec4 sampleGradient(float t) const;

		FontAtlas* m_font = nullptr;
		Font*      m_fontChain = nullptr;   // set only for the scale-aware ctor
		float      m_logicalSize = 0.0f;    // logical pixels, scale-aware ctor only
		std::string m_text;
		glm::vec4 m_color;

		std::vector<GradientStop> m_gradientStops;
		GradientAxis m_gradientAxis = GradientAxis::Horizontal;
		float m_gradientSpeed = 0.0f;   // ramp positions per second
		float m_gradientPhase = 0.0f;   // wrapped scroll offset, in ramp positions
	};

}
