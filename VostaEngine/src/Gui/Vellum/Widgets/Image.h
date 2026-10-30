#pragma once

#include "Core/Core.h"
#include "Gui/Vellum/Core/DrawList.h"   // TextureId
#include "Gui/Vellum/Core/Widget.h"

namespace ve::vellum {

	// One texture stretched over its rect, drawn with tint and an optional
	// sub-rect. No tiling: use NineGridImage when the art is a 3x3 panel.
	class VE_API Image : public Widget {
	public:
		Image() = default;

		// `sourceSizePx` is the sampled region in texels; it becomes the desired
		// size for an axis left at 0 in setSize(), i.e. the natural size.
		void setTexture(TextureId texture, const glm::vec2& sourceSizePx);

		// Desired size per axis. <= 0 on an axis falls back to the source size.
		void setSize(const glm::vec2& size);
		const glm::vec2& size() const { return m_size; }

		void setTint(const glm::vec4& color) { m_tint = color; }
		const glm::vec4& tint() const { return m_tint; }

		// Atlas sub-rect. Defaults to the whole texture.
		void setUV(const glm::vec2& uv0, const glm::vec2& uv1);

		bool hasTexture() const { return m_hasTexture; }

		void arrange(const Rect& slot) override;
		void paint(PaintContext& ctx) override;

	protected:
		glm::vec2 computeDesiredSize() const override;

	private:
		// 0 is the renderer's white pixel, so "unset" needs its own flag.
		TextureId m_texture = 0;
		bool      m_hasTexture = false;
		glm::vec2 m_sourceSize{ 0.0f };
		glm::vec2 m_uv0{ 0.0f };
		glm::vec2 m_uv1{ 1.0f };
		glm::vec2 m_size{ 0.0f };
		glm::vec4 m_tint{ 1.0f };
	};

}
