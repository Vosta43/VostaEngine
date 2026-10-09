#pragma once
#include <VostaEngine.h>

#include "Renderer/LayeredMaterial.h"

#include <functional>
#include <string>

namespace ve {

	// Standalone editor window for a LayeredMaterial asset. The material is a
	// shared asset, so this edits the asset itself: every terrain referencing it
	// sees the change, and edits are written back to the same file. It is the
	// counterpart of MaterialGraphPanel, which edits the single-surface kind.
	class LayeredMaterialEditor {
	public:
		// Invoked when the user opens a layer's own asset from the stack.
		using OpenLayerFn = std::function<void(AssetHandle)>;
		void setOnOpenLayer(OpenLayerFn callback) { m_onOpenLayer = std::move(callback); }

		// Binds the panel to a layered-material .veasset path (registry-relative
		// or absolute) and shows the window.
		void openFile(const std::string& path);
		// Binds the panel to an already-registered handle (e.g. a terrain slot).
		void openHandle(AssetHandle handle);
		void onGuiRender(bool* openFlag = nullptr);

	private:
		void markDirty();
		void saveToFile();

		// The live registered material; edits mutate it in place so the renderer
		// sees them without a reload. Null until a file is opened.
		Ref<LayeredMaterial> m_material;
		AssetHandle m_handle;
		OpenLayerFn m_onOpenLayer;
		// Registry key (== save path). It is authoritative: re-keyed on rename or
		// move, whereas m_material->name in the file body can be stale.
		std::string m_key;
	};

}
