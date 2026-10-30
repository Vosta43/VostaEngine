#pragma once

#include "Core/Core.h"
#include "Gui/Vellum/Core/DrawList.h"   // TextureId
#include "Gui/Vellum/Core/Widget.h"

#include <glm.hpp>

namespace ve::vellum {

	// One texture drawn as a uniform 3x3 block grid, tiled at a fixed block size
	// until it covers the slot. Blocks are never stretched and never cut: the
	// target is rounded up to whole blocks, so the draw can overhang its slot by
	// up to cellSize-1 on each axis but the source's nine cells always land
	// whole. A slot smaller than 3x3 blocks still draws the full grid.
	//
	// Not a nine-slice: a slice stretches its edges and centre, this repeats them.
	class VE_API NineGridImage : public Widget {
	public:
		NineGridImage() = default;

		// `sourceSizePx` is the sampled region in texels. It seeds the default
		// block size (the shorter side / 3, i.e. 1:1) and the grid's minimum.
		void setTexture(TextureId texture, const glm::vec2& sourceSizePx);

		// On-screen size of one block, rounded to whole pixels. <= 0 restores the
		// size derived from the source.
		void setCellSize(float pixels);
		float cellSize() const { return m_cell; }

		// Floor for the desired size, like ColoredBox's size.
		void setSize(const glm::vec2& size);

		void setTint(const glm::vec4& color) { m_tint = color; }
		const glm::vec4& tint() const { return m_tint; }

		// Atlas sub-rect. Defaults to the whole texture.
		void setUV(const glm::vec2& uv0, const glm::vec2& uv1);

		bool hasTexture() const { return m_hasTexture; }

		// The grid actually drawn: whole blocks, at least 3x3. Valid after arrange.
		glm::vec2 drawnSize() const { return m_rect.size; }

		void arrange(const Rect& slot) override;
		void paint(PaintContext& ctx) override;

	protected:
		glm::vec2 computeDesiredSize() const override;

	private:
		void resolveCell();

		// 0 is the renderer's white pixel, so "unset" needs its own flag.
		TextureId m_texture = 0;
		bool      m_hasTexture = false;
		glm::vec2 m_sourceSize{ 0.0f };
		glm::vec2 m_uv0{ 0.0f };
		glm::vec2 m_uv1{ 1.0f };
		glm::vec2 m_size{ 0.0f };      // desired-size floor
		glm::vec4 m_tint{ 1.0f };
		float     m_cellSize = 0.0f;   // requested, <= 0 = derive from source
		float     m_cell = 1.0f;       // resolved, whole pixels
	};

}
