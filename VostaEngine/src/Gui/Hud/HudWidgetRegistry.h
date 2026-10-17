#pragma once

#include "Core/Core.h"
#include "Core/Json.h"
#include "Gui/Vellum/Core/Widget.h"

#include <functional>
#include <string>
#include <vector>

namespace ve::vellum {
	class FontAtlas;
}

namespace ve {

	// What a widget needs from the host while a document is being instantiated.
	// Supplied by the caller (the HUD system borrows VellumLayer's fonts) so the
	// document itself stays font-agnostic.
	struct HudBuildContext {
		// Resolve an atlas for a requested pixel size. Never null in a working host.
		std::function<vellum::FontAtlas* (float)> atlasFor;

		vellum::FontAtlas* atlas(float size) const {
			return atlasFor ? atlasFor(size) : nullptr;
		}
	};

	// One widget kind, the single truth the HUD serializer, the property panel and
	// the runtime instantiator all read from. Adding a kind = adding an entry here;
	// existing documents keep loading because an unknown tag is skipped, not fatal.
	struct HudWidgetType {
		std::string tag;      // document "type" token, e.g. "button"
		std::string schema;   // editor hint: "size(size), color(color), label(string)"
		// Default-constructed instance; props arrive separately through apply().
		Ref<vellum::Widget> (*create)();
		// Fill the instance's properties from the node's "props" object.
		void (*apply)(vellum::Widget&, const JsonReader& props, const HudBuildContext&);
	};

	const std::vector<HudWidgetType>& hudWidgetTypes();

	// Null when `tag` is not a known kind.
	const HudWidgetType* findHudWidgetType(const std::string& tag);

}
