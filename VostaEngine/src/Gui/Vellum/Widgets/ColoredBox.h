#pragma once

#include "Core/Core.h"
#include "Gui/Vellum/Core/Widget.h"

namespace ve::vellum {

// Paints its rect with a solid color, then lets children draw on top.
// It has no intrinsic size, so it hugs its content and takes `size` as a floor —
// the same fallback Slate's SBorder has via its image size. A box with neither
// content nor an explicit size desires nothing and will not show up.
class VE_API ColoredBox : public Widget {
public:
	explicit ColoredBox(const glm::vec4& color, const glm::vec2& size = glm::vec2(0.0f));

	void setColor(const glm::vec4& color) { m_color = color; }
	const glm::vec4& color() const { return m_color; }

	void setSize(const glm::vec2& size);

	void arrange(const Rect& slot) override;
	void paint(PaintContext& ctx) override;

protected:
	glm::vec2 computeDesiredSize() const override;

private:
	glm::vec4 m_color;
	glm::vec2 m_size{ 0.0f };
};

}
