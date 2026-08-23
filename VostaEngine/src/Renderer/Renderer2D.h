#pragma once

#include "Texture.h"
#include <glm.hpp>

#include "SubTexture2D.h"
#include "Core/AssetHandle.h"
#include "Shader.h"

namespace ve {
	class VE_API Renderer2D {
	public:
		static void init();

		static void beginScene(const glm::mat4& viewProjectionMatrix);
		static void beginPickingScene(const glm::mat4& viewProjectionMatrix, const Ref<Shader>& pickingShader);
		static void endPickingScene();
		static void endScene();
		// Flush any remaining batched geometry at the end of the frame.
		// If no draw calls were issued this frame, flush() is a no-op.
		static void flush();

		static void drawQuad(const glm::mat4& transform, const glm::vec2& size, AssetHandle textureHandle, const glm::vec4& tint = glm::vec4(1.0f));

		static void drawQuad(const glm::vec3& position, const glm::vec2& size, const Ref<SubTexture2D>& subTexture, const glm::vec4& color);
		static void drawQuad(const glm::vec3& position, const glm::vec2& size, AssetHandle textureHandle, const glm::vec4& color);

		static void drawPickingQuad(const glm::vec3& position, const glm::vec2& size, uint32_t entityID);
		static void drawPickingQuad(const glm::mat4& transform, const glm::vec2& size, uint32_t entityID);

		static void setBlending(bool enabled);

	private:

	};
}