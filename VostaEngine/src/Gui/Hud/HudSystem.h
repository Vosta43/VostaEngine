#pragma once

#include "Core/Core.h"
#include "Gui/Hud/HudInstance.h"
#include "Scene/ECSystems/ECSystem.h"

#include <cstdint>
#include <unordered_map>

namespace ve {

	class Scene;

	// Turns authored HudComponents into live HudInstances and hands the tree to
	// the Vellum layer to draw. One instance per HUD entity, built lazily on the
	// first tick that sees it.
	class VE_API HudSystem : public ECSystemBase {
	public:
		explicit HudSystem(Scene& scene);

		void onUpdate(float deltaTime) override;

		// The instance built for a HUD entity, or null before the first tick.
		Ref<HudInstance> instance(uint32_t entityId) const;

	private:
		Scene& m_scene;
		std::unordered_map<uint32_t, Ref<HudInstance>> m_instances;
	};

}
