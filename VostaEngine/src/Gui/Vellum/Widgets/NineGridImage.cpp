#include "vepch.h"
#include "NineGridImage.h"
#include "Gui/Vellum/Core/PaintContext.h"

#include <algorithm>
#include <cmath>

namespace ve::vellum {

	void NineGridImage::setTexture(TextureId texture, const glm::vec2& sourceSizePx) {
		m_texture = texture;
		m_hasTexture = texture != 0;
		m_sourceSize = sourceSizePx;
		resolveCell();
		invalidateDesiredSize();
	}

	void NineGridImage::setCellSize(float pixels) {
		if (pixels == m_cellSize)
			return;
		m_cellSize = pixels;
		resolveCell();
		invalidateDesiredSize();
	}

	void NineGridImage::setSize(const glm::vec2& size) {
		if (size == m_size)
			return;
		m_size = size;
		invalidateDesiredSize();
	}

	void NineGridImage::setUV(const glm::vec2& uv0, const glm::vec2& uv1) {
		m_uv0 = uv0;
		m_uv1 = uv1;
	}

	void NineGridImage::resolveCell() {
		// Blocks are square on screen, so a non-square source is resampled per
		// axis; the shorter side keeps the grid inside the source at 1:1.
		const float sourceCell = std::min(m_sourceSize.x, m_sourceSize.y) / 3.0f;
		const float cell = m_cellSize > 0.0f ? m_cellSize : sourceCell;
		m_cell = std::max(1.0f, std::round(cell));
	}

	glm::vec2 NineGridImage::computeDesiredSize() const {
		if (!m_hasTexture)
			return m_size;

		const float minimum = 3.0f * m_cell;
		return {
			std::max(m_size.x, minimum),
			std::max(m_size.y, minimum)
		};
	}

	void NineGridImage::arrange(const Rect& slot) {
		const float cell = m_cell;
		const float width = std::max(slot.size.x, 3.0f * cell);
		const float height = std::max(slot.size.y, 3.0f * cell);

		// Round up: a block is never cut, so the draw covers the slot and may
		// overhang it by less than one cell on the right and bottom edges.
		const float cols = std::max(3.0f, std::ceil(width / cell));
		const float rows = std::max(3.0f, std::ceil(height / cell));

		m_rect = { slot.pos, { cols * cell, rows * cell } };
	}

	void NineGridImage::paint(PaintContext& ctx) {
		if (!m_hasTexture)
			return;

		const float cell = m_cell;
		const int cols = static_cast<int>(std::max(3.0f, std::ceil(m_rect.size.x / cell)));
		const int rows = static_cast<int>(std::max(3.0f, std::ceil(m_rect.size.y / cell)));

		// Each block samples one third of the source on each axis. The class of a
		// row or column picks the third, which is what turns corners into single
		// cells, edges into one repeated third, and the middle into both.
		const glm::vec2 span = (m_uv1 - m_uv0) / 3.0f;

		for (int r = 0; r < rows; ++r) {
			const int ri = r == 0 ? 0 : (r == rows - 1 ? 2 : 1);
			for (int k = 0; k < cols; ++k) {
				const int ci = k == 0 ? 0 : (k == cols - 1 ? 2 : 1);
				const glm::vec2 uv0 = m_uv0 + glm::vec2(span.x * ci, span.y * ri);
				const Rect block{ { m_rect.pos.x + k * cell, m_rect.pos.y + r * cell },
				                  { cell, cell } };

				ctx.drawQuad(block, uv0, uv0 + span, m_texture, m_tint);
			}
		}

		Widget::paint(ctx);
	}

}
