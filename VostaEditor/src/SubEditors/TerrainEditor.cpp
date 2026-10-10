#include "TerrainEditor.h"

#include <ext/matrix_projection.hpp>

#include <algorithm>
#include <cmath>

#include "imgui.h"

#include "Scene/Terrain/Terrain.h"
#include "Renderer/LayeredMaterial.h"
#include "Renderer/MaterialLayerAsset.h"
#include "Renderer/Texture.h"
#include "Asset/TextureResource.h"

namespace ve {

	namespace {

		// Height-sculpt palette, in enum order matching TerrainEditor::HeightTool.
		const char* kHeightToolIcons[] = {
			"VostaEditor/resources/icons/terrain_raise.png",
			"VostaEditor/resources/icons/terrain_lower.png",
			"VostaEditor/resources/icons/terrain_smooth.png",
			"VostaEditor/resources/icons/terrain_flatten.png",
			"VostaEditor/resources/icons/terrain_sharpen.png",
			"VostaEditor/resources/icons/terrain_erosion.png",
		};
		const char* kHeightToolNames[] = {
			"Raise", "Lower", "Smooth", "Flatten", "Sharpen", "Erosion",
		};

		std::string layerName(AssetHandle h) {
			auto a = ResourceManager::get<MaterialLayerAsset>(h);
			return a ? a->layer.name : std::string();
		}

		// Every layer asset is created named "Layer"; treat that (and an empty
		// name) as unnamed, or the palette repeats the same word on every row.
		bool isPlaceholderName(const std::string& n) {
			return n.empty() || n == "Layer";
		}

		// The layer's albedo as a palette swatch. A layer has no baked preview of
		// its own, and its identity is its albedo.
		void drawLayerSwatch(AssetHandle layerAsset, float size) {
			AssetHandle albedo;
			if (auto a = ResourceManager::get<MaterialLayerAsset>(layerAsset))
				albedo = a->layer.albedoMap;

			auto tex = ResourceManager::get<Texture2D>(albedo);
			if (tex && tex->getRendererID() != 0)
				ImGui::Image((ImTextureID)(uintptr_t)tex->getRendererID(), ImVec2(size, size));
			else
				ImGui::Dummy(ImVec2(size, size));   // keeps the row text aligned
		}

		// Brush shape preview: the radial white-centre-to-black-edge falloff that
		// Terrain::paintWeight writes, so the head's softness is visible before
		// painting (Unity's paint tools show the same swatch).
		Ref<Texture2D> buildBrushThumb(int n) {
			auto res = CreateRef<TextureResource>();
			res->width = (uint32_t)n;
			res->height = (uint32_t)n;
			res->format = TextureFormat::RGBA;
			res->pixels.resize((size_t)n * n * 4);

			const float r = n * 0.5f;
			for (int y = 0; y < n; ++y) {
				for (int x = 0; x < n; ++x) {
					const float dx = (x + 0.5f) - r;
					const float dy = (y + 0.5f) - r;
					const float dd = std::min((dx * dx + dy * dy) / (r * r), 1.0f);
					const float f = (1.0f - dd) * (1.0f - dd);
					const unsigned char v = (unsigned char)std::lround(f * 255.0f);
					const size_t o = ((size_t)y * n + x) * 4;
					res->pixels[o + 0] = v;
					res->pixels[o + 1] = v;
					res->pixels[o + 2] = v;
					res->pixels[o + 3] = 255;
				}
			}
			return Texture2D::create(res);
		}

		// World ray through a viewport pixel (y-down, viewport-local coords).
		void viewportRay(const glm::mat4& view, const glm::mat4& proj,
		                 const glm::vec2& mouseViewportPos, const glm::vec2& viewportSize,
		                 glm::vec3& outOrigin, glm::vec3& outDir) {
			const glm::vec4 vp(0.0f, 0.0f, viewportSize.x, viewportSize.y);
			const float flippedY = viewportSize.y - mouseViewportPos.y;

			const glm::vec3 nearP = glm::unProject(
				glm::vec3(mouseViewportPos.x, flippedY, 0.0f), view, proj, vp);
			const glm::vec3 farP = glm::unProject(
				glm::vec3(mouseViewportPos.x, flippedY, 1.0f), view, proj, vp);

			outOrigin = nearP;
			outDir = glm::normalize(farP - nearP);
		}

		// Icon-only tool toggle in the toolbar style: the icon tints dark on a light
		// plate while active, and the name lives in the tooltip so the tool strip
		// stays compact. Returns true on the click that flips the state.
		bool iconToggle(Ref<Texture2D>& icon, const char* id, const char* tooltip,
		                bool active, float size) {
			const ImVec2 p0 = ImGui::GetCursorScreenPos();
			const bool clicked = ImGui::InvisibleButton(id, ImVec2(size, size));
			const bool hovered = ImGui::IsItemHovered();
			const ImU32 bg = active ? IM_COL32(215, 215, 215, 255)
				: hovered ? ImGui::GetColorU32(ImGuiCol_ButtonHovered)
				          : ImGui::GetColorU32(ImGuiCol_Button);
			const ImU32 tint = active ? IM_COL32(0, 0, 0, 255) : IM_COL32(255, 255, 255, 255);

			ImDrawList* dl = ImGui::GetWindowDrawList();
			dl->AddRectFilled(p0, ImVec2(p0.x + size, p0.y + size), bg, 3.0f);
			if (icon && icon->getRendererID())
				dl->AddImageRounded((ImTextureID)(uintptr_t)icon->getRendererID(),
					p0, ImVec2(p0.x + size, p0.y + size),
					ImVec2(0, 1), ImVec2(1, 0), tint, 3.0f);
			if (hovered)
				ImGui::SetTooltip("%s", tooltip);
			return clicked;
		}

	}

	void TerrainEditor::ensureIcon() {
		if (!m_icon)
			m_icon = Texture2D::create("VostaEditor/resources/icons/terrain_brush.png");
	}

	void TerrainEditor::ensureMountainIcon() {
		if (!m_mountainIcon)
			m_mountainIcon = Texture2D::create("VostaEditor/resources/icons/mountain.png");
	}

	void TerrainEditor::ensureBrushThumb() {
		if (!m_brushThumb)
			m_brushThumb = buildBrushThumb(64);
	}

	void TerrainEditor::ensureHeightToolIcons() {
		for (int i = 0; i < kHeightToolCount; ++i) {
			if (!m_heightToolIcons[i])
				m_heightToolIcons[i] = Texture2D::create(kHeightToolIcons[i]);
		}
	}

	void TerrainEditor::drawInspector(bool& active, Scene& scene) {
		// Tool strip that used to sit in the viewport toolbar: painting is a property
		// of the terrain being edited, so the switches belong next to the terrain.
		// Same icon-button look as the toolbar toggles, sized up to read as the
		// section's title. Each name lives in its own tooltip.
		ensureIcon();
		ensureMountainIcon();
		ensureBrushThumb();
		{
			const float h = ImGui::GetFrameHeight() * 1.6f;
			if (iconToggle(m_icon, "##paintTexture", "Paint Texture", active, h))
				active = !active;
			ImGui::SameLine();
			if (iconToggle(m_mountainIcon, "##mountainTool", "Mountain", m_mountainActive, h))
				m_mountainActive = !m_mountainActive;
		}

		// Height-sculpt palette: picking an operation only selects it for now, the
		// strokes it would drive are not implemented yet.
		if (m_mountainActive) {
			ensureHeightToolIcons();
			const float s = ImGui::GetFrameHeight() * 1.25f;
			ImGui::Indent();
			for (int i = 0; i < kHeightToolCount; ++i) {
				if (i > 0)
					ImGui::SameLine();
				ImGui::PushID(i);
				const bool selected = (int)m_heightTool == i;
				if (iconToggle(m_heightToolIcons[i], "##heightTool", kHeightToolNames[i], selected, s))
					m_heightTool = (HeightTool)i;
				ImGui::PopID();
			}
			ImGui::Unindent();
		}

		if (!active)
			return;

		// The brush acts on the whole terrain through the scene's system, so no
		// tile selection is needed: aiming only decides *where* a stroke lands.
		TerrainSystemComponent* sys = Terrain::findSystem(scene);
		if (!sys) {
			ImGui::TextDisabled("No terrain system in this scene.");
			return;
		}

		// The layer palette reads the scene's shared terrain material; a tile no
		// longer carries its own.
		auto layered = std::dynamic_pointer_cast<LayeredMaterial>(
			ResourceManager::get<Material>(sys->materialHandle));

		ImGui::Indent();
		// Brush shape preview: the radial falloff stamp the stroke writes, so the
		// head's softness is visible before painting (Unity's paint tools swatch).
		if (m_brushThumb && m_brushThumb->getRendererID())
			ImGui::Image((ImTextureID)(uintptr_t)m_brushThumb->getRendererID(), ImVec2(64, 64));
		ImGui::SameLine();
		ImGui::BeginGroup();
		ImGui::SetNextItemWidth(200.0f);
		ImGui::SliderFloat("Size", &m_settings.radius, 0.5f, 100.0f, "%.1f m");
		ImGui::SetNextItemWidth(200.0f);
		ImGui::SliderFloat("Strength", &m_settings.strength, 0.0f, 1.0f, "%.2f");
		ImGui::EndGroup();

		// Palette rows are numbered exactly like the material editor's list, so the
		// two read the same: material layer i is the control map's channel i-1.
		// Layer 0 is the auto-fill base and has no channel, hence not selectable.
		const int count = layered ? (int)layered->layers.size() : 0;
		const int paintable = std::min(count > 0 ? count - 1 : 0, 3);

		if (m_settings.layer > paintable) m_settings.layer = paintable;
		if (m_settings.layer < 1) m_settings.layer = 1;

		ImGui::TextUnformatted("Layers");
		if (!layered)
			ImGui::TextDisabled("Assign a Layered Material to paint layers.");
		else if (count == 0)
			ImGui::TextDisabled("No layers yet; add some in the material editor.");
		else if (count == 1)
			ImGui::TextDisabled("Only the auto-fill base; add a layer to paint one.");

		for (int i = 0; i < count && i <= 3; ++i) {
			const AssetHandle lh = layered->layers[i];
			const bool isBase = (i == 0);
			const std::string nm = layerName(lh);

			std::string label = "Layer " + std::to_string(i);
			if (!isPlaceholderName(nm)) label += "  " + nm;
			if (isBase) label += "   (base - auto-fill)";
			else if (!lh.isValid()) label += "   (empty)";

			ImGui::PushID(i);
			if (isBase) ImGui::BeginDisabled();
			drawLayerSwatch(lh, 20.0f);
			ImGui::SameLine();
			if (ImGui::Selectable(label.c_str(), !isBase && m_settings.layer == i))
				m_settings.layer = i;
			if (isBase) ImGui::EndDisabled();
			ImGui::PopID();
		}

		ImGui::Spacing();
		ImGui::BeginDisabled(paintable < 1);
		if (ImGui::Button("Fill", ImVec2(70, 0))) {
			Terrain::fillWeight(scene, m_settings.layer);
			m_status = "Filled Layer " + std::to_string(m_settings.layer);
		}
		ImGui::SameLine();
		if (ImGui::Button("Clear", ImVec2(70, 0))) {
			Terrain::clearWeight(scene, m_settings.layer);
			m_status = "Cleared Layer " + std::to_string(m_settings.layer);
		}
		ImGui::EndDisabled();
		ImGui::SameLine();
		ImGui::TextDisabled("%s", m_status.c_str());
		ImGui::Unindent();
	}

	bool TerrainEditor::raycastTerrain(const glm::mat4& view, const glm::mat4& proj,
	                                   const glm::vec2& mouseViewportPos, const glm::vec2& viewportSize,
	                                   Scene& scene,
	                                   uint32_t& outEntity, glm::vec3& outWorldPos) {
		if (viewportSize.x <= 0.0f || viewportSize.y <= 0.0f)
			return false;

		glm::vec3 rayO, rayD;
		viewportRay(view, proj, mouseViewportPos, viewportSize, rayO, rayD);

		return Terrain::raycastScene(scene, rayO, rayD, 100000.0f, outEntity, outWorldPos);
	}

	bool TerrainEditor::hoverPoint(const glm::mat4& view, const glm::mat4& proj,
	                               const glm::vec2& mouseViewportPos, const glm::vec2& viewportSize,
	                               Scene& scene,
	                               glm::vec3& outWorldPos, glm::vec3& outNormal) {
		uint32_t entity = UINT32_MAX;
		if (!raycastTerrain(view, proj, mouseViewportPos, viewportSize, scene,
				entity, outWorldPos))
			return false;

		auto& tc = scene.getComponent<TerrainComponent>(entity);
		auto& tr = scene.getComponent<TransformComponent>(entity);
		const glm::vec3 t(tr.transform[3]);
		outNormal = Terrain::surfaceNormal(scene, tc, outWorldPos.x - t.x, outWorldPos.z - t.z);
		return true;
	}

	bool TerrainEditor::paintAt(const glm::mat4& view, const glm::mat4& proj,
	                            const glm::vec2& mouseViewportPos, const glm::vec2& viewportSize,
	                            Scene& scene) {
		if (m_settings.radius <= 0.0f || m_settings.strength <= 0.0f)
			return false;

		uint32_t entity = UINT32_MAX;
		glm::vec3 world;
		if (!raycastTerrain(view, proj, mouseViewportPos, viewportSize, scene,
				entity, world)) {
			m_status = "Cursor is off the terrain";
			return false;
		}

		Terrain::paintWeightWorld(scene, entity, world, m_settings.layer,
		                          m_settings.radius, m_settings.strength);
		return true;
	}

}
