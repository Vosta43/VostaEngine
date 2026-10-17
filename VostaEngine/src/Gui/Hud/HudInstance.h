#pragma once

#include "Core/Core.h"
#include "Gui/Hud/HudAsset.h"

#include "Gui/Vellum/Core/Widget.h"
#include "Gui/Vellum/Widgets/Button.h"
#include "Gui/Vellum/Widgets/ColoredBox.h"
#include "Gui/Vellum/Widgets/TextBlock.h"

#include <string>
#include <unordered_map>

namespace ve {

	// A built HUD: the instantiated Vellum tree plus a name -> widget index, so
	// runtime code reaches widgets by name. The tree is handed to a renderer
	// (VellumLayer); the instance owns no frame driver of its own. One asset can
	// build many instances.
	//
	//   if (auto* b = hud->button("StartButton"))
	//       b->setOnClick([this]{ startGame(); });
	class VE_API HudInstance {
	public:
		static Ref<HudInstance> create(const Ref<HudAsset>& asset, const HudBuildContext& ctx);

		vellum::Widget* root() const { return m_root.get(); }
		const Ref<vellum::Widget>& rootRef() const { return m_root; }

		// Null when no node carries that name.
		vellum::Widget* find(const std::string& name) const;
		// Typed lookup; null on a miss or a type mismatch.
		template<typename T>
		T* find(const std::string& name) const {
			return dynamic_cast<T*>(this->find(name));
		}

		vellum::Button* button(const std::string& name) const { return find<vellum::Button>(name); }
		vellum::TextBlock* text(const std::string& name) const { return find<vellum::TextBlock>(name); }
		vellum::ColoredBox* box(const std::string& name) const { return find<vellum::ColoredBox>(name); }

	private:
		void index(vellum::Widget* widget);

		Ref<vellum::Widget> m_root;
		std::unordered_map<std::string, vellum::Widget*> m_byName;
	};

}
