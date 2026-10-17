#pragma once

#include <VostaEngine.h>
#include "Graph/NodeGraphPanel.h"
#include "Graph/TerrainRuleEditorSchema.h"
#include "Renderer/Texture.h"

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace ve {

// Standalone terrain-rule editor: a node graph (Terrain Attribute / Noise Asset /
// noise math / Rule Output) plus a live preview per output. Each Rule Output gets a
// grayscale thumbnail baked over a synthetic height field, so the author sees where a
// layer's weight lands before assigning the graph to a terrain.
class TerrainRulePanel {
public:
	// Binds the panel to a terrain_rule .veasset and opens it with that file's graph.
	void openFile(const std::string& path);

	void onGuiRender(bool* openFlag = nullptr);

private:
	// Seeds the default Terrain Attribute (Height) -> Rule Output (layer 1) graph when
	// none is loaded.
	void ensureDefaultGraph();
	void loadGraphFromDisk();
	// Re-evaluates the graph over the synthetic field and refreshes every output
	// thumbnail. Hash-gated by the caller, so it runs only when the graph moved.
	void rebuildPreview();
	void saveToFile();
	void markDirty();

	RuleGraph               m_graph;
	NodeGraphPanel          m_canvas{ "TerrainRuleCanvas" };
	TerrainRuleEditorSchema m_schema;

	static constexpr int kThumb = 96;

	// One preview thumbnail per layer the bands route into, in first-seen order.
	struct OutputView {
		int                  layer = 1;
		Ref<Texture2D>       tex;
		std::vector<uint8_t> pixels;
	};
	std::vector<OutputView> m_outputs;

	// Synthetic height field the preview evaluates over, laid out [x * res + z] so it
	// matches what a Terrain Attribute node expects.
	std::vector<float> m_heightField;

	uint32_t m_graphHash = 0;
	uint32_t m_savedHash = 0;

	// Shared with the registered saver, so its own write reads back as ours.
	std::shared_ptr<int64_t> m_diskTicks = std::make_shared<int64_t>(0);
	bool m_externalChange = false;

	// Empty when the panel is a scratch pad; edits are then not persisted.
	std::string m_filePath;
};

}
