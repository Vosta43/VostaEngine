#include "vepch.h"
#include "Gui/Hud/HudAsset.h"

#include "Core/Log.h"
#include "Scene/Archive.h"

#include "Gui/Vellum/Widgets/Button.h"
#include "Gui/Vellum/Widgets/Stack.h"

namespace ve {

	namespace {

		HudNode readNode(const JsonReader& r) {
			HudNode n;
			n.type = r.getString("type", std::string());
			n.name = r.getString("name", std::string());
			n.anchor = r.getVec2("anchor", glm::vec2(0.0f));
			n.offset = r.getVec2("offset", glm::vec2(0.0f));
			n.props = r.has("props") ? r.rawJson("props") : std::string("{}");

			const size_t count = r.arraySize("children");
			n.children.reserve(count);
			for (size_t i = 0; i < count; ++i)
				n.children.push_back(readNode(r.at("children", i)));
			return n;
		}

		// Writes a node's body into the already-open enclosing object.
		void writeNodeBody(const HudNode& n, JsonWriter& w) {
			w.set("type", n.type);
			if (!n.name.empty())
				w.set("name", n.name);
			w.set("anchor", n.anchor);
			w.set("offset", n.offset);
			w.setRaw("props", n.props);

			if (!n.children.empty()) {
				w.beginArray("children", n.children.size());
				for (const HudNode& c : n.children) {
					w.beginObject();
					writeNodeBody(c, w);
					w.end();
				}
				w.end();
			}
		}

		// Builds one node and its subtree. An unknown tag is skipped with a warning
		// rather than failing the whole document, so a file from a newer editor
		// still loads the parts this build understands.
		Ref<vellum::Widget> buildNode(const HudNode& n, const HudBuildContext& ctx) {
			const HudWidgetType* type = findHudWidgetType(n.type);
			if (!type) {
				VE_CORE_WARN_PRINT("HUD: unknown widget type '%s' (node '%s')",
					n.type.c_str(), n.name.c_str());
				return nullptr;
			}

			Ref<vellum::Widget> widget = type->create();
			if (!widget)
				return nullptr;

			widget->setName(n.name);
			JsonReader props;
			JsonReader::parse(n.props, props);
			type->apply(*widget, props, ctx);

			// A Stack positions the children it is given by anchor/offset; a Button
			// centres them. Any other kind ignores children.
			vellum::Stack* stack = dynamic_cast<vellum::Stack*>(widget.get());
			const bool centresChildren = dynamic_cast<vellum::Button*>(widget.get()) != nullptr;

			for (const HudNode& child : n.children) {
				Ref<vellum::Widget> built = buildNode(child, ctx);
				if (!built)
					continue;
				if (stack)
					stack->add(built, child.offset, child.anchor);
				else if (centresChildren)
					widget->addChild(built);
			}

			return widget;
		}

	}

	Ref<HudAsset> HudAsset::create(const std::string& path) {
		TextArchive ar(path, ArchiveMode::read);
		if (!ar.isGood())
			return nullptr;

		std::string token;
		int32_t version = 0;
		ar >> token;
		ar >> version;
		if (token != kTypeTag || version != kFormatVersion)
			return nullptr;

		std::string body;
		ar >> body;
		return fromJson(body);
	}

	Ref<HudAsset> HudAsset::fromJson(const std::string& jsonText) {
		auto asset = CreateRef<HudAsset>();
		if (!asset->loadFromJson(jsonText))
			return nullptr;
		return asset;
	}

	bool HudAsset::loadFromJson(const std::string& jsonText) {
		JsonReader doc;
		if (!JsonReader::parse(jsonText, doc))
			return false;
		if (!doc.has("root"))
			return false;

		referenceResolution = doc.getVec2("referenceResolution", referenceResolution);
		root = readNode(doc.child("root"));
		return !root.type.empty();
	}

	std::string HudAsset::toJson() const {
		JsonWriter w;
		w.set("referenceResolution", referenceResolution);
		w.beginObject("root");
		writeNodeBody(root, w);
		w.end();
		return w.str();
	}

	bool HudAsset::save(const std::string& path) const {
		TextArchive ar(path, ArchiveMode::write);
		if (!ar.isGood())
			return false;
		ar << std::string(kTypeTag);
		ar << static_cast<int32_t>(kFormatVersion);
		ar << toJson();
		return true;
	}

	Ref<vellum::Widget> HudAsset::build(const HudBuildContext& ctx) const {
		return buildNode(root, ctx);
	}

}
