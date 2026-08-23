#include "vepch.h"
#include "SubTexture2D.h"

namespace ve {
	SubTexture2D::SubTexture2D(const Ref<Texture2D>& texture, const glm::vec2& min, const glm::vec2& max)
		: m_texture(texture){
		
		m_texCoord[0] = { min.x,min.y };
		m_texCoord[1] = { max.x,min.y };
		m_texCoord[2] = { max.x,max.y };
		m_texCoord[3] = { min.x,max.y };

	}

    Ref<SubTexture2D> SubTexture2D::createFromCoords(
        const Ref<Texture2D>& texture,
        const glm::vec2& coords,     
        const glm::vec2& cellSize,   
        const glm::vec2& spriteSize) 
    {
        glm::vec2 min = {
            (coords.x * cellSize.x) / texture->getWidth(),
            (coords.y * cellSize.y) / texture->getHeight()
        };
        glm::vec2 max = {
            ((coords.x + spriteSize.x) * cellSize.x) / texture->getWidth(),  
            ((coords.y + spriteSize.y) * cellSize.y) / texture->getHeight()  
        };

        return CreateRef<SubTexture2D>(texture, min, max);
    }

}

