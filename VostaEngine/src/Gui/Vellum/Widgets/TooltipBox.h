#pragma once

#include "Core/Core.h"
#include "Gui/Vellum/Core/Widget.h"
#include "Gui/Vellum/Widgets/TextBlock.h"

#include <glm.hpp>

namespace ve::vellum {

	class FontAtlas;

	// Text bubble shown by WidgetTree over the hovered widget. It is not part of the
	// routed tree: the tree lays it out against the surface and paints it after the
	// root, so panel clips cannot cut it and it never steals hover from the widget
	// it describes. `anchor` is where the bubble hangs from (the cursor), and the
	// whole bubble is clamped into the surface.
	class VE_API TooltipBox : public Widget {
	public:
		explicit TooltipBox(FontAtlas* font = nullptr);

		void setFont(FontAtlas* font);
		void setText(const std::string& text);
		const std::string& text() const { return m_text->text(); }

		void setAnchor(const glm::vec2& anchor) { m_anchor = anchor; }
		void setOffset(const glm::vec2& offset) { m_offset = offset; }
		void setPadding(const glm::vec2& padding);
		void setBackgroundColor(const glm::vec4& color) { m_background = color; }
		void setBorderColor(const glm::vec4& color) { m_border = color; }
		void setTextColor(const glm::vec4& color);

		void arrange(const Rect& slot) override;
		void paint(PaintContext& ctx) override;

	protected:
		glm::vec2 computeDesiredSize() const override;

	private:
		Ref<TextBlock> m_text;
		glm::vec2      m_anchor{ 0.0f };
		glm::vec2      m_offset{ 14.0f, 20.0f };
		glm::vec2      m_padding{ 8.0f, 5.0f };
		glm::vec4      m_background{ 0.09f, 0.10f, 0.12f, 0.96f };
		glm::vec4      m_border{ 0.36f, 0.38f, 0.44f, 1.0f };
		glm::vec4      m_textColor{ 0.92f, 0.93f, 0.95f, 1.0f };
	};

}
