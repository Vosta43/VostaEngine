#include "vepch.h"
#include "Gui/Hud/HudSystem.h"

#include "Core/Application.h"
#include "Core/ResourceManager.h"
#include "Gui/Vellum/VellumLayer.h"
#include "Scene/Components.h"
#include "Scene/Scene.h"

namespace ve {

	HudSystem::HudSystem(Scene& scene)
		: m_scene(scene) {}

	Ref<HudInstance> HudSystem::instance(uint32_t entityId) const {
		auto it = m_instances.find(entityId);
		return it != m_instances.end() ? it->second : nullptr;
	}

	void HudSystem::onUpdate(float) {
		vellum::VellumLayer* layer = Application::get().getVellumLayer();
		if (!layer)
			return;

		HudBuildContext ctx;
		ctx.atlasFor = [layer](float size) { return layer->font(size); };

		for (uint32_t id : m_scene.getRegistry().view<HudComponent>()) {
			if (m_instances.count(id))
				continue;

			auto& comp = m_scene.getComponent<HudComponent>(id);
			if (!comp.hudHandle.isValid())
				continue;

			Ref<HudAsset> asset = ResourceManager::get<HudAsset>(comp.hudHandle);
			if (!asset)
				continue;

			Ref<HudInstance> built = HudInstance::create(asset, ctx);
			if (!built)
				continue;

			m_instances[id] = built;
			// The Vellum layer draws a single tree; the most recently built HUD
			// owns it. Multiple HUDs at once is a later (sub-rect / stacked) concern.
			layer->setRoot(built->rootRef());
		}
	}

}
