#include "vepch.h"
#include "Gui/Hud/HudInstance.h"

namespace ve {

	void HudInstance::index(vellum::Widget* widget) {
		if (!widget)
			return;
		if (!widget->name().empty())
			m_byName[widget->name()] = widget;
		for (const auto& child : widget->children())
			index(child.get());
	}

	Ref<HudInstance> HudInstance::create(const Ref<HudAsset>& asset, const HudBuildContext& ctx) {
		if (!asset)
			return nullptr;

		auto instance = CreateRef<HudInstance>();
		instance->m_root = asset->build(ctx);
		if (!instance->m_root)
			return nullptr;

		instance->index(instance->m_root.get());
		return instance;
	}

	vellum::Widget* HudInstance::find(const std::string& name) const {
		auto it = m_byName.find(name);
		return it != m_byName.end() ? it->second : nullptr;
	}

}
