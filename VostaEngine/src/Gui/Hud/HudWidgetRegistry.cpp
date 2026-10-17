#include "vepch.h"
#include "Gui/Hud/HudWidgetRegistry.h"

#include "Gui/Vellum/Widgets/Button.h"
#include "Gui/Vellum/Widgets/ColoredBox.h"
#include "Gui/Vellum/Widgets/Stack.h"
#include "Gui/Vellum/Widgets/TextBlock.h"

namespace ve {

	namespace {

		using vellum::Button;
		using vellum::ColoredBox;
		using vellum::Stack;
		using vellum::TextBlock;
		using vellum::Widget;

		Ref<Widget> createStack() { return CreateRef<Stack>(); }
		Ref<Widget> createBox() { return CreateRef<ColoredBox>(glm::vec4(1.0f), glm::vec2(0.0f)); }
		Ref<Widget> createText() { return CreateRef<TextBlock>(nullptr, std::string()); }
		Ref<Widget> createButton() { return CreateRef<Button>(glm::vec2(0.0f)); }

		// A Stack carries no properties of its own; its children hold their own
		// placement (anchor/offset), read by the tree builder, not here.
		void applyStack(Widget&, const JsonReader&, const HudBuildContext&) {}

		void applyBox(Widget& w, const JsonReader& p, const HudBuildContext&) {
			auto& box = static_cast<ColoredBox&>(w);
			box.setColor(p.getVec4("color", box.color()));
			box.setSize(p.getVec2("size", glm::vec2(0.0f)));
		}

		void applyText(Widget& w, const JsonReader& p, const HudBuildContext& ctx) {
			auto& t = static_cast<TextBlock&>(w);
			t.setFont(ctx.atlas(p.getFloat("fontSize", 24.0f)));
			t.setText(p.getString("text", std::string()));
			t.setColor(p.getVec4("color", glm::vec4(1.0f)));
		}

		void applyButton(Widget& w, const JsonReader& p, const HudBuildContext&) {
			auto& b = static_cast<Button&>(w);
			b.setSize(p.getVec2("size", glm::vec2(0.0f)));
			b.setColor(p.getVec4("color", glm::vec4(0.23f, 0.25f, 0.29f, 1.0f)));
			if (p.has("hoverColor"))
				b.setHoverColor(p.getVec4("hoverColor"));
			if (p.has("pressedColor"))
				b.setPressedColor(p.getVec4("pressedColor"));
		}

	}

	const std::vector<HudWidgetType>& hudWidgetTypes() {
		static const std::vector<HudWidgetType> types = {
			{ "stack",       "",                                                                 createStack,  applyStack  },
			{ "colored_box", "size(size), color(color)",                                         createBox,    applyBox    },
			{ "text",        "text(string), color(color), fontSize(float,min=8,max=200)",         createText,   applyText   },
			{ "button",      "size(size), color(color), hoverColor(color), pressedColor(color)", createButton, applyButton },
		};
		return types;
	}

	const HudWidgetType* findHudWidgetType(const std::string& tag) {
		for (const HudWidgetType& t : hudWidgetTypes())
			if (tag == t.tag)
				return &t;
		return nullptr;
	}

}
