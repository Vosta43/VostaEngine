#pragma once

#include <glm.hpp>
#include "Texture.h"

namespace ve {
	class VE_API SubTexture2D {
	public:
		SubTexture2D(const Ref<Texture2D>& texture, const glm::vec2& min, const glm::vec2& max);

		const Ref<Texture2D> getTexture() const {return m_texture;}
		const glm::vec2* getTexCoord() const {return m_texCoord;}

		static Ref<SubTexture2D> createFromCoords(const Ref<Texture2D>& texture, const glm::vec2& coords, const glm::vec2& cellSize, const glm::vec2& spriteSize);
	private:

		Ref<Texture2D> m_texture;
		glm::vec2 m_texCoord[4];
	};

}