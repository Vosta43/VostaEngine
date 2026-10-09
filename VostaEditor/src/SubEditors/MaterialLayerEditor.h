#pragma once
#include <VostaEngine.h>

#include "Renderer/MaterialLayerAsset.h"

#include <string>

namespace ve {

	// Editor window for a standalone MaterialLayerAsset. The layer is a shared
	// asset, so this edits the asset itself: every material referencing it sees
	// the change, and edits are written back to the same file.
	class MaterialLayerEditor {
	public:
		// Binds the panel to a material-layer .veasset and shows the window.
		void openFile(const std::string& path);
		void onGuiRender(bool* openFlag = nullptr);

	private:
		void saveToFile();
		// Register a self-contained save with the dirty registry (captures the
		// path + asset Ref, so it survives this window closing).
		void markDirty();

		// The live registered asset; edits mutate it in place so the renderer sees
		// them without a reload. Null until a file is opened.
		Ref<MaterialLayerAsset> m_asset;
		std::string m_path;
	};

}
