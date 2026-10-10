#pragma once

#include "Core/Core.h"
#include "Gui/Vellum/Core/Widget.h"

#include <glm.hpp>
#include <string>

namespace ve::vellum {

	class FontAtlas;

	// A single line of UTF-8 text drawn with a FontAtlas. The font is borrowed, not
	// owned, and is non-const because a first-time codepoint rasterises into it on
	// demand. One line only — no wrapping, no newlines.
	class VE_API TextBlock : public Widget {
	public:
		explicit TextBlock(FontAtlas* font,
		                   const std::string& text,
		                   const glm::vec4& color = glm::vec4(1.0f));

		void setText(const std::string& text);
		const std::string& text() const { return m_text; }
		void setColor(const glm::vec4& color) { m_color = color; }

		void arrange(const Rect& slot) override;
		void paint(PaintContext& ctx) override;

	protected:
		glm::vec2 computeDesiredSize() const override;

	private:
		FontAtlas* m_font = nullptr;
		std::string m_text;
		glm::vec4 m_color;
	};

}
