#include "vepch.h"
#include "Image.h"
#include "Gui/Vellum/Core/PaintContext.h"

namespace ve::vellum {

	void Image::setTexture(TextureId texture, const glm::vec2& sourceSizePx) {
		m_texture = texture;
		m_hasTexture = texture != 0;
		m_sourceSize = sourceSizePx;
		invalidateDesiredSize();
	}

	void Image::setSize(const glm::vec2& size) {
		if (size == m_size)
			return;
		m_size = size;
		invalidateDesiredSize();
	}

	void Image::setUV(const glm::vec2& uv0, const glm::vec2& uv1) {
		m_uv0 = uv0;
		m_uv1 = uv1;
	}

	glm::vec2 Image::computeDesiredSize() const {
		const glm::vec2 natural = m_hasTexture ? m_sourceSize : glm::vec2(0.0f);
		return {
			m_size.x > 0.0f ? m_size.x : natural.x,
			m_size.y > 0.0f ? m_size.y : natural.y
		};
	}

	void Image::arrange(const Rect& slot) {
		m_rect = slot;
	}

	void Image::paint(PaintContext& ctx) {
		if (!m_hasTexture)
			return;

		ctx.drawQuad(m_rect, m_uv0, m_uv1, m_texture, m_tint);
		Widget::paint(ctx);
	}

}
