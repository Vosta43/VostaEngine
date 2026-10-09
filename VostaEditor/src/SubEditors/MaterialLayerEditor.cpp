#include "MaterialLayerEditor.h"

#include <cstdio>
#include <filesystem>

#include "imgui.h"

#include "AssetSaveRegistry.h"
#include "Core/AssetConfig.h"
#include "Core/ResourceManager.h"
#include "Panels/AssetBrowserWidget.h"
#include "Renderer/Texture.h"

namespace ve {

	namespace {

		// Thumbnail for the layer's texture combo rows.
		void drawTexturePreview(AssetHandle h, float size) {
			auto tex = ResourceManager::get<Texture2D>(h);
			if (tex && tex->getRendererID() != 0) {
				ImGui::Image((ImTextureID)(uintptr_t)tex->getRendererID(), ImVec2(size, size));
			} else {
				ImGui::Button("?", ImVec2(size, size));
			}
		}

	}

	void MaterialLayerEditor::openFile(const std::string& path) {
		m_path = path;

		AssetHandle h = ResourceManager::find<MaterialLayerAsset>(path);
		if (!h.isValid())
			h = ResourceManager::store<MaterialLayerAsset>(path);
		m_asset = ResourceManager::get<MaterialLayerAsset>(h);
	}

	void MaterialLayerEditor::onGuiRender(bool* openFlag) {
		const std::string title = m_path.empty()
			? std::string("Material Layer###materialLayerEditor")
			: "Material Layer - " + std::filesystem::path(m_path).stem().string() + "###materialLayerEditor";

		ImGui::SetNextWindowSize(ImVec2(420.0f, 460.0f), ImGuiCond_FirstUseEver);
		if (!ImGui::Begin(title.c_str(), openFlag)) {
			ImGui::End();
			return;
		}

		if (!m_asset) {
			ImGui::TextDisabled("No layer loaded.");
			ImGui::End();
			return;
		}

		MaterialLayer& layer = m_asset->layer;

		bool edited = false;

		char nameBuf[128];
		std::snprintf(nameBuf, sizeof(nameBuf), "%s", layer.name.c_str());
		if (ImGui::InputText("Name", nameBuf, sizeof(nameBuf))) {
			layer.name = nameBuf;
			edited = true;
		}

		edited |= AssetBrowser::ResourceCombo<Texture2D>("Albedo", layer.albedoMap, drawTexturePreview, 24.0f);
		edited |= AssetBrowser::ResourceCombo<Texture2D>("Normal", layer.normalMap, drawTexturePreview, 24.0f);
		edited |= AssetBrowser::ResourceCombo<Texture2D>("Roughness", layer.roughnessMap, drawTexturePreview, 24.0f);
		edited |= AssetBrowser::ResourceCombo<Texture2D>("Height", layer.heightMap, drawTexturePreview, 24.0f);
		ImGui::TextDisabled("Roughness / Height are optional (default 1.0).");
		ImGui::TextDisabled("The engine packs albedo+height and normal+roughness.");
		edited |= ImGui::DragFloat("Tiling", &layer.tiling, 0.1f, 0.1f, 1000.0f);

		const char* blendNames[] = { "Weight Blend", "Alpha Blend", "Height Blend" };
		int blendIdx = (int)layer.blend;
		if (ImGui::Combo("Blend", &blendIdx, blendNames, 3)) {
			layer.blend = (MaterialLayerBlend)blendIdx;
			edited = true;
		}

		edited |= ImGui::Checkbox("Slope Rule", &layer.useSlope);
		if (layer.useSlope) {
			edited |= ImGui::DragFloat("Slope Min", &layer.slopeMin, 0.01f, 0.0f, 1.0f);
			edited |= ImGui::DragFloat("Slope Max", &layer.slopeMax, 0.01f, 0.0f, 1.0f);
		}

		edited |= ImGui::Checkbox("Height Rule", &layer.useHeight);
		if (layer.useHeight) {
			edited |= ImGui::DragFloat("Height Min", &layer.heightMin, 0.01f, 0.0f, 1.0f);
			edited |= ImGui::DragFloat("Height Max", &layer.heightMax, 0.01f, 0.0f, 1.0f);
		}

		edited |= ImGui::DragFloat("Boundary Noise", &layer.noiseStrength, 0.01f, 0.0f, 1.0f);
		edited |= ImGui::DragFloat("Noise Scale", &layer.noiseScale, 1.0f, 1.0f, 500.0f);

		if (edited)
			markDirty();

		ImGui::Separator();
		if (ImGui::Button("Save", ImVec2(120.0f, 0.0f)))
			saveToFile();
		ImGui::SameLine();
		if (AssetSaveRegistry::get().isDirty(toAbsolute(m_path)))
			ImGui::TextColored(ImVec4(0.95f, 0.75f, 0.30f, 1.0f), "unsaved");
		else
			ImGui::TextDisabled("%s", m_path.empty() ? "(unbound)" : m_path.c_str());

		ImGui::End();
	}

	void MaterialLayerEditor::markDirty() {
		if (!m_asset || m_path.empty())
			return;

		// Capture the path and the asset Ref, not `this`: the registry outlives
		// this window, and the Ref keeps the resource alive until it is written.
		// TextArchive opens against the process CWD, so resolve the project path
		// first; m_path is not guaranteed to be absolute.
		auto asset = m_asset;
		const std::string path = toAbsolute(m_path);
		AssetSaveRegistry::get().markDirty(path, [path, asset]() {
			TextArchive ar(path, ArchiveMode::write);
			if (ar.isGood())
				asset->serialize(ar);
		});
	}

	void MaterialLayerEditor::saveToFile() {
		if (!m_asset || m_path.empty())
			return;

		const std::string path = toAbsolute(m_path);
		TextArchive ar(path, ArchiveMode::write);
		if (!ar.isGood()) {
			VE_CORE_WARN_PRINT("MaterialLayer save failed: %s", path.c_str());
			return;
		}
		m_asset->serialize(ar);
		AssetSaveRegistry::get().markClean(path);
	}

}
