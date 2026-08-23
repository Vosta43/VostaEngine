#include "TerrainEditor.h"

#include <ext/matrix_projection.hpp>

namespace ve {

	void TerrainEditor::onLeftClicked(const glm::vec2& mouseViewportPos){

		if (!m_cameraController) {
			VE_CORE_ERROR_PRINT("Terrain editor haven't initialized!");
			return;
		}

		auto bounds = Application::get().getGuiLayer()->getViewportBounds();
		auto& camera = m_cameraController->getCamera();
		
		glm::vec3 rayDir = getRayDir(mouseViewportPos,bounds.second,camera.getViewMatrix(),camera.getProjectionMatrix());
		glm::vec3 cameraPos = camera.getPosition();

		const float stepSize = 0.1f;
		const float maxDistance = 10000.0f;

		for (float t = 0.0f; t < maxDistance; t += stepSize) {
			
			glm::vec3 checkPos = cameraPos + rayDir * t;
			float checkHeight = checkPos.y;
			
		}
	}

	glm::vec3 TerrainEditor::getRayDir(const glm::vec2& mouseViewportPos,
	                                    const glm::vec2& viewportSize,
	                                    const glm::mat4& viewMatrix,
	                                    const glm::mat4& projMatrix) {
		glm::vec4 viewport(0.0f, 0.0f, viewportSize.x, viewportSize.y);

		// Flip Y: ImGui top-left origin → OpenGL bottom-left origin
		float flippedY = viewportSize.y - mouseViewportPos.y;

		glm::vec3 nearPoint = glm::unProject(
			glm::vec3(mouseViewportPos.x, flippedY, 0.0f),
			viewMatrix, projMatrix, viewport);

		glm::vec3 farPoint = glm::unProject(
			glm::vec3(mouseViewportPos.x, flippedY, 1.0f),
			viewMatrix, projMatrix, viewport);

		return glm::normalize(farPoint - nearPoint);
	}

}
