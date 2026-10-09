#include "LayeredMaterialEditor.h"

#include <filesystem>

#include "imgui.h"

#include "AssetSaveRegistry.h"
#include "Core/AssetConfig.h"
#include "Core/ResourceManager.h"
#include "Panels/AssetBrowserWidget.h"
#include "Renderer/MaterialLayer.h"
#include "Renderer/MaterialLayerAsset.h"
#include "Renderer/ThumbnailRenderer.h"
#include "Scene/Archive.h"

namespace ve {

	namespace {

		void drawLayerThumbnail(AssetHandle h, float size) {
			auto thumbnail = ThumbnailRenderer::getMaterialThumbnail(h);
			if (thumbnail && thumbnail->getRendererID() != 0) {
				ImGui::Image((ImTextureID)(uintptr_t)thumbnail->getRendererID(), ImVec2(size, size));
			} else {
				ImGui::Button("?", ImVec2(size, size));
			}
		}

	}

	void LayeredMaterialEditor::openFile(const std::string& path) {
		AssetHandle h = ResourceManager::find<Material>(path);
		if (!h.isValid())
			h = ResourceManager::store<Material>(path);
		openHandle(h);
	}

	void LayeredMaterialEditor::openHandle(AssetHandle handle) {
		m_handle = handle;
		m_material = std::dynamic_pointer_cast<LayeredMaterial>(ResourceManager::get<Material>(handle));

		// Registry key is the authoritative save path, so a rename/move does not
		// leave us writing to a stale name.
		m_key = ResourceManager::getPath<Material>(handle);
		if (m_key.empty() && m_material)
			m_key = m_material->assetPath();
	}

	void LayeredMaterialEditor::onGuiRender(bool* openFlag) {
		const std::string title = m_key.empty()
			? std::string("Layered Material###layeredMaterialEditor")
			: "Layered Material - " + std::filesystem::path(m_key).stem().string() + "###layeredMaterialEditor";

		ImGui::SetNextWindowSize(ImVec2(460.0f, 520.0f), ImGuiCond_FirstUseEver);
		if (!ImGui::Begin(title.c_str(), openFlag)) {
			ImGui::End();
			return;
		}

		if (!m_material) {
			ImGui::TextDisabled("No layered material loaded.");
			ImGui::End();
			return;
		}

		// Preview sphere, matching the single-material editor's layout.
		if (m_handle.isValid()) {
			auto thumbnail = ThumbnailRenderer::getMaterialThumbnail(m_handle);
			if (thumbnail && thumbnail->getRendererID() != 0) {
				ImGui::Image((ImTextureID)(uintptr_t)thumbnail->getRendererID(),
				             ImVec2(160.0f, 160.0f), ImVec2(0, 1), ImVec2(1, 0));
				ImGui::SameLine();
			}
		}

		ImGui::BeginGroup();
		ImGui::Text("Layers (%d/%d)", (int)m_material->layers.size(), kMaxMaterialLayers);
		ImGui::TextDisabled("Layer 0 is the base: it fills whatever the others leave.");
		ImGui::EndGroup();

		ImGui::Separator();

		// --- Layer stack: pick which shared MaterialLayer assets blend, in order.
		// Each asset's own rule and textures are edited in its own window (Edit).
		int moveFrom = -1, moveTo = -1, removeIndex = -1;
		bool edited = false;

		for (int i = 0; i < (int)m_material->layers.size(); ++i) {
			ImGui::PushID(i);
			ImGui::AlignTextToFramePadding();
			ImGui::Text("Layer %d", i);

			ImGui::SameLine();
			if (ImGui::SmallButton("Up") && i > 0) { moveFrom = i; moveTo = i - 1; }
			ImGui::SameLine();
			if (ImGui::SmallButton("Down") && i + 1 < (int)m_material->layers.size()) { moveFrom = i; moveTo = i + 1; }
			ImGui::SameLine();
			if (ImGui::SmallButton("X")) { removeIndex = i; }

			AssetHandle& slot = m_material->layers[i];
			edited |= AssetBrowser::ResourceCombo<MaterialLayerAsset>("##layerAsset", slot, drawLayerThumbnail);

			// A layer's own rule and maps live in its shared asset, edited in that
			// asset's own window; this stack only chooses which ones blend, in order.
			ImGui::SameLine();
			ImGui::BeginDisabled(!slot.isValid());
			if (ImGui::Button("Edit") && slot.isValid() && m_onOpenLayer)
				m_onOpenLayer(slot);
			ImGui::EndDisabled();
			ImGui::PopID();
		}

		if (moveFrom >= 0) {
			std::swap(m_material->layers[moveFrom], m_material->layers[moveTo]);
			edited = true;
		}
		if (removeIndex >= 0) {
			m_material->layers.erase(m_material->layers.begin() + removeIndex);
			edited = true;
		}

		if (m_material->layers.size() < (size_t)kMaxMaterialLayers) {
			if (ImGui::Button("Add Layer", ImVec2(-1, 0))) {
				// Empty slot; the user picks a shared MaterialLayer asset for it.
				m_material->layers.push_back(INVALID_ASSET_HANDLE);
				edited = true;
			}
		} else {
			ImGui::TextDisabled("Max %d layers", kMaxMaterialLayers);
		}

		ImGui::Separator();

		// u_HeightScale maps world Y into the 0..1 bands the layer rules use, so it
		// must equal the terrain's own Height Scale or the height rules drift.
		edited |= ImGui::DragFloat("Material Height Scale", &m_material->heightScale, 0.1f, 0.1f, 10000.0f);
		edited |= ImGui::DragFloat("Macro Strength", &m_material->macroStrength, 0.01f, 0.0f, 1.0f);
		edited |= ImGui::DragFloat("Macro Scale", &m_material->macroScale, 1.0f, 1.0f, 10000.0f);

		if (edited)
			markDirty();

		ImGui::Separator();
		if (ImGui::Button("Save", ImVec2(120.0f, 0.0f)))
			saveToFile();
		ImGui::SameLine();
		if (AssetSaveRegistry::get().isDirty(m_key))
			ImGui::TextColored(ImVec4(0.95f, 0.75f, 0.30f, 1.0f), "unsaved");
		else
			ImGui::TextDisabled("%s", m_key.empty() ? "(unbound)" : m_key.c_str());

		ImGui::End();
	}

	void LayeredMaterialEditor::markDirty() {
		if (!m_material || m_key.empty())
			return;

		// Capture the path and the material Ref, not `this`: the registry outlives
		// this window, and the Ref keeps the resource alive until it is written.
		auto material = m_material;
		const std::string path = toAbsolute(m_key);
		AssetSaveRegistry::get().markDirty(m_key, [path, material]() {
			TextArchive ar(path, ArchiveMode::write);
			if (ar.isGood())
				material->serialize(ar);
		});
	}

	void LayeredMaterialEditor::saveToFile() {
		if (!m_material || m_key.empty())
			return;

		const std::string path = toAbsolute(m_key);
		TextArchive ar(path, ArchiveMode::write);
		if (!ar.isGood()) {
			VE_CORE_WARN_PRINT("LayeredMaterial save failed: %s", path.c_str());
			return;
		}
		m_material->serialize(ar);
		AssetSaveRegistry::get().markClean(m_key);

		// The asset changed on disk, so the cached sphere is stale.
		if (m_handle.isValid())
			ThumbnailRenderer::invalidate(m_handle);
	}

}
