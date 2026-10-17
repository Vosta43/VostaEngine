#pragma once

#include "Core/Core.h"
#include "Core/Json.h"
#include "Gui/Hud/HudWidgetRegistry.h"

#include <cstdint>
#include <glm.hpp>
#include <string>
#include <vector>

namespace ve {

	// One authored node: the registry tag, its placement under the parent, its
	// type-specific properties (kept as raw JSON so the widget registry alone
	// parses them) and its children.
	struct HudNode {
		std::string type;                       // registry tag, e.g. "button"
		std::string name;                       // unique in the document, for binding
		glm::vec2 anchor{ 0.0f, 0.0f };         // parent-relative placement
		glm::vec2 offset{ 0.0f, 0.0f };
		std::string props = "{}";               // raw JSON object
		std::vector<HudNode> children;
	};

	// A HUD document. Loads through ResourceManager and instantiates to a Vellum
	// tree. On-disk format is the engine's text-asset convention: the "hud" token,
	// a version int, then the JSON body -- so AssetLibrary classifies it by token
	// like every other .veasset.
	class VE_API HudAsset {
	public:
		static constexpr const char* kTypeTag = "hud";
		static constexpr int32_t kFormatVersion = 1;

		// Factory for ResourceManager::store<HudAsset>.
		static Ref<HudAsset> create(const std::string& path);
		// Parse a document straight from JSON text (in-code HUDs, tests).
		static Ref<HudAsset> fromJson(const std::string& jsonText);

		bool save(const std::string& path) const;

		// Instantiate the tree. Null when the root is missing or its tag is unknown.
		Ref<vellum::Widget> build(const HudBuildContext& ctx) const;

		std::string toJson() const;

		glm::vec2 referenceResolution{ 1920.0f, 1080.0f };
		HudNode root;

	private:
		bool loadFromJson(const std::string& jsonText);
	};

}
