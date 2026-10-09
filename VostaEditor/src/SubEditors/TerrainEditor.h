#pragma once
#include <VostaEngine.h>

#include <glm.hpp>
#include <string>

namespace ve {

	// Settings for the terrain material paint brush.
	struct TerrainBrushSettings {
		float radius = 8.0f;    // world units
		float strength = 1.0f;  // 0..1 cap on the weight a stroke writes
		int   layer = 1;        // control-map channel: 1..3 -> r,g,b
	};

	// Terrain material paint brush. Owns the paint settings, draws the brush
	// section of the TerrainSystemComponent inspector (Unity's Paint Texture tool),
	// and applies strokes across the whole terrain by raycasting it.
	class TerrainEditor {
	public:
		// Height-sculpt operations offered by the height tool palette. Selection is
		// UI-only for now — none of these are wired to the terrain yet.
		enum class HeightTool { Raise, Lower, Smooth, Flatten, Sharpen, Erosion };

		// Draw the brush controls inline, into whatever window is current. `active`
		// is the caller's "painting enabled" flag, toggled from the section header.
		// The brush acts on the whole terrain, so no tile selection is needed.
		void drawInspector(bool& active, Scene& scene);

		// Paint one dab at the viewport cursor. Returns true when a stroke landed.
		// The stroke lands wherever the cursor ray meets the terrain, spanning as
		// many tiles as the brush overlaps.
		bool paintAt(const glm::mat4& view, const glm::mat4& proj,
		             const glm::vec2& mouseViewportPos, const glm::vec2& viewportSize,
		             Scene& scene);

		// World-space point where the cursor ray meets the terrain, plus the surface
		// normal there, or false when the cursor is not over it. Drives the viewport
		// brush-cursor ring.
		bool hoverPoint(const glm::mat4& view, const glm::mat4& proj,
		                const glm::vec2& mouseViewportPos, const glm::vec2& viewportSize,
		                Scene& scene, glm::vec3& outWorldPos, glm::vec3& outNormal);

		float radius() const { return m_settings.radius; }

	private:
		// March the cursor ray against every terrain tile in the scene, nearest hit
		// wins. False when the cursor is not over any terrain.
		bool raycastTerrain(const glm::mat4& view, const glm::mat4& proj,
		                    const glm::vec2& mouseViewportPos, const glm::vec2& viewportSize,
		                    Scene& scene, uint32_t& outEntity, glm::vec3& outWorldPos);

		void ensureIcon();
		void ensureMountainIcon();
		void ensureBrushThumb();
		void ensureHeightToolIcons();

		static constexpr int kHeightToolCount = 6;

		TerrainBrushSettings m_settings;
		Ref<Texture2D> m_icon;
		// Second tool icon in the header strip; its behaviour is not wired yet, so
		// the toggle only holds its own on/off state.
		Ref<Texture2D> m_mountainIcon;
		bool m_mountainActive = false;
		Ref<Texture2D> m_heightToolIcons[kHeightToolCount];
		HeightTool m_heightTool = HeightTool::Raise;
		Ref<Texture2D> m_brushThumb;
		std::string m_status = "LMB drag to paint";
	};

}
