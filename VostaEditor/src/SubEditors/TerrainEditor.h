#pragma once
#include <VostaEngine.h>

#include <glm.hpp>

namespace ve {

	enum class BrushType {
		None,
		Raise,
		Flatten
	};

	struct BrushSetting {
		float radius = 1.0f;
		float strength = 0.1f;
		BrushType type = BrushType::Raise;
	};


	class TerrainEditor {
	public:
		void init(const Ref<CameraController>& cameraController) {m_cameraController = cameraController;};
		void activate() { m_activated = true;}
		void inactivate() {m_activated = false;}

		void onLeftClicked(const glm::vec2& mouseViewportPos);

		static glm::vec3 getRayDir(const glm::vec2& mouseViewportPos,
		                           const glm::vec2& viewportSize,
		                           const glm::mat4& viewMatrix,
		                           const glm::mat4& projMatrix);

	private:
		bool m_activated = false;
		BrushSetting m_brushSetting;
		Ref<CameraController> m_cameraController;

	};


}
