#include "TerrainRulePanel.h"

#include "AssetSaveRegistry.h"
#include "Asset/Utils.h"
#include "Core/AssetConfig.h"
#include "Noise/NoiseCPU.h"
#include "Noise/NoiseSettings.h"
#include "Scene/Archive.h"
#include "Terrain/RuleNodes.h"
#include "Terrain/RulePlan.h"
#include "Terrain/RuleGraphResource.h"
#include "Core/Log.h"
#include "Renderer/MaterialLayer.h"

#include <imgui.h>

#include <algorithm>
#include <filesystem>

namespace ve {

namespace {

	// Must match TerrainRuleGraphResource's reader and the file browser's writer.
	constexpr int32_t kRuleFormatVersion = 2;

	// FNV-1a over node ids, every kind's parameters, and the links. Changes exactly
	// when the graph's shape or a parameter does, which gates the preview and the save.
	uint32_t hashGraph(const RuleGraph& g) {
		uint32_t h = 2166136261u;
		auto mix = [&](const void* data, size_t n) {
			const uint8_t* p = static_cast<const uint8_t*>(data);
			for (size_t i = 0; i < n; ++i) { h ^= p[i]; h *= 16777619u; }
		};

		for (const auto& node : g.nodes) {
			mix(&node->id, sizeof(node->id));
			auto mixf = [&](float v) { mix(&v, sizeof(float)); };

			if (auto* src = dynamic_cast<FieldSourceNode*>(node.get())) {
				const int32_t m = static_cast<int32_t>(src->mode);
				mix(&m, sizeof(m));
				mix(src->noisePath.data(), src->noisePath.size());
			} else if (auto* bands = dynamic_cast<LayerBandsNode*>(node.get())) {
				const int32_t n = (int32_t)bands->segments.size();
				mix(&n, sizeof(n));
				for (const LayerSegment& s : bands->segments) {
					mixf(s.minValue); mixf(s.maxValue); mixf(s.soft);
					const int32_t l = s.layer;
					mix(&l, sizeof(l));
				}
			}
		}
		for (const auto& link : g.links) {
			mix(&link.startPin.id, sizeof(uint32_t));
			mix(&link.startPin.pinIndex, sizeof(uint32_t));
			mix(&link.endPin.id, sizeof(uint32_t));
			mix(&link.endPin.pinIndex, sizeof(uint32_t));
		}
		return h;
	}

}

void TerrainRulePanel::ensureDefaultGraph() {
	if (!m_graph.nodes.empty()) return;

	// Height split into three slices, so the default does something visible the moment
	// it is assigned and shows the shape (a source feeding a bands node) the user edits.
	auto src   = CreateRef<FieldSourceNode>();
	auto bands = CreateRef<LayerBandsNode>();
	src->mode = FieldSourceNode::Mode::Height;
	m_graph.addNode(src,   glm::vec2(-280.0f, 0.0f));
	m_graph.addNode(bands, glm::vec2(60.0f, 0.0f));
	m_graph.addLink(src->m_outputPins[0].id, bands->m_inputPins[0].id);
}

void TerrainRulePanel::onGuiRender(bool* openFlag) {
	ensureDefaultGraph();

	// Did the file change on disk since we read it? Reload in place when we hold no
	// edits, else flag a conflict. Runs before Begin so a reload shares this frame.
	if (!m_filePath.empty()) {
		const int64_t liveTicks = utils::fileWriteTicks(toAbsolute(m_filePath));
		if (liveTicks != 0 && liveTicks != *m_diskTicks) {
			*m_diskTicks = liveTicks;
			if (!AssetSaveRegistry::get().isDirty(m_filePath)) {
				loadGraphFromDisk();
				m_savedHash = m_graphHash;
				m_externalChange = false;
			} else {
				m_externalChange = true;
			}
		}
	}

	const std::string title = m_filePath.empty()
		? std::string("Terrain Rule Editor###terrainRuleEditor")
		: "Terrain Rule Editor - " + std::filesystem::path(m_filePath).stem().string()
			+ "###terrainRuleEditor";

	ImGui::SetNextWindowSize(ImVec2(980.0f, 620.0f), ImGuiCond_FirstUseEver);
	if (!ImGui::Begin(title.c_str(), openFlag)) {
		ImGui::End();
		return;
	}

	// --- Left panel: one preview per output + details ---
	ImGui::BeginChild("##ruleOutputs", ImVec2((float)kThumb + 72.0f, 0.0f));
	ImGui::TextUnformatted("Weights");
	if (m_outputs.empty())
		ImGui::TextDisabled("Add a Layer Bands node.");
	for (const OutputView& ov : m_outputs) {
		ImGui::Image((void*)(intptr_t)ov.tex->getRendererID(),
			ImVec2((float)kThumb, (float)kThumb));
		ImGui::Text("Layer %d", ov.layer);
	}

	// The asset changed on disk while we have unsaved edits. Never pick a winner
	// silently: reload (dropping our edits) or keep ours (to be saved over disk).
	if (m_externalChange) {
		ImGui::Separator();
		ImGui::TextColored(ImVec4(1.0f, 0.55f, 0.2f, 1.0f), "Changed on disk");
		const float half = ((float)kThumb + 72.0f) * 0.5f - 8.0f;
		if (ImGui::Button("Reload##ruleExternal", ImVec2(half, 0.0f))) {
			loadGraphFromDisk();
			AssetSaveRegistry::get().markClean(m_filePath);
			m_savedHash = m_graphHash;
			*m_diskTicks = utils::fileWriteTicks(toAbsolute(m_filePath));
			m_externalChange = false;
		}
		ImGui::SameLine();
		if (ImGui::Button("Keep mine##ruleExternal", ImVec2(half, 0.0f))) {
			*m_diskTicks = utils::fileWriteTicks(toAbsolute(m_filePath));
			m_externalChange = false;
		}
	}

	ImGui::Separator();
	if (GraphNode* selected = m_graph.findNode(m_canvas.selectedNodeId()))
		m_schema.drawDetailsContent(selected);
	else
		ImGui::TextDisabled("No node selected");
	ImGui::EndChild();

	ImGui::SameLine();

	// --- Right panel: generic node canvas ---
	m_canvas.draw(m_graph, m_schema);

	const uint32_t h = hashGraph(m_graph);
	if (h != m_graphHash) {
		m_graphHash = h;
		rebuildPreview();
		markDirty();
	}
	if (!m_filePath.empty() && h != m_savedHash && !ImGui::IsAnyItemActive()) {
		saveToFile();
		m_savedHash = h;
		AssetSaveRegistry::get().markClean(m_filePath);
	}

	ImGui::End();
}

void TerrainRulePanel::loadGraphFromDisk() {
	m_graph.nodes.clear();
	m_graph.links.clear();
	m_schema.resetNodeState();

	TextArchive ar(m_filePath, ArchiveMode::read);
	if (ar.isGood()) {
		std::string token;
		ar >> token;   // "terrain_rule"
		int32_t version = 0;
		ar >> version;
		if (version == kRuleFormatVersion)
			deserializeRuleGraph(m_graph, ar);
	}

	ensureDefaultGraph();   // fall back if the file was empty or legacy
	rebuildPreview();
}

void TerrainRulePanel::openFile(const std::string& path) {
	m_filePath = path;
	loadGraphFromDisk();
	m_savedHash = m_graphHash;   // don't re-save what was just read
	*m_diskTicks = utils::fileWriteTicks(toAbsolute(m_filePath));
	m_externalChange = false;
	AssetSaveRegistry::get().markClean(m_filePath);
}

void TerrainRulePanel::markDirty() {
	if (m_filePath.empty()) return;

	// Snapshot the graph: the registry outlives this panel, and the save must not
	// depend on the panel still being open when it runs.
	const std::string path = m_filePath;
	const RuleGraph graph = m_graph;
	auto diskTicks = m_diskTicks;   // shared: this write must not read back as external
	AssetSaveRegistry::get().markDirty(path, [path, graph, diskTicks]() {
		TextArchive ar(path, ArchiveMode::write);
		if (!ar.isGood())
			return;
		ar << std::string("terrain_rule");
		ar << kRuleFormatVersion;
		serializeRuleGraph(graph, ar);
		TerrainRuleGraphResource::refreshFromFile(path);
		*diskTicks = utils::fileWriteTicks(toAbsolute(path));
	});
}

void TerrainRulePanel::saveToFile() {
	TextArchive ar(m_filePath, ArchiveMode::write);
	if (!ar.isGood()) {
		VE_CORE_WARN_PRINT("Terrain rule save failed: %s", m_filePath.c_str());
		return;
	}

	ar << std::string("terrain_rule");
	ar << kRuleFormatVersion;
	serializeRuleGraph(m_graph, ar);

	// Keep the registered resource in step, so a terrain re-bakes from the edits
	// without an editor restart.
	TerrainRuleGraphResource::refreshFromFile(m_filePath);
	*m_diskTicks = utils::fileWriteTicks(toAbsolute(m_filePath));
}

void TerrainRulePanel::rebuildPreview() {
	const int N = kThumb;

	// A synthetic field so the preview works before the graph is ever on a terrain.
	// One fBm field in [0,1]; a Height attribute then reads back something patterned.
	if ((int)m_heightField.size() != N * N) {
		m_heightField.assign((size_t)N * N, 0.0f);
		NoiseSettings ns;
		ns.fractal   = NoiseSettings::Fractal::Fbm;
		ns.octaves   = 4;
		ns.frequency = 0.02f;
		const float half = (float)N * 0.5f;
		for (int z = 0; z < N; ++z)
			for (int x = 0; x < N; ++x) {
				const float v = NoiseCPU::sample(ns, (float)x - half, (float)z - half);
				m_heightField[(size_t)x * N + z] = std::clamp(v * 0.5f + 0.5f, 0.0f, 1.0f);
			}
	}

	// One view per layer the bands route into, reusing an existing texture per layer so
	// a re-bake does not churn GL objects. No upper bound: whatever the graph references
	// gets a tile, even a layer past what the material can currently hold — dropping it
	// silently would hide a mis-authored band.
	std::vector<int> layers;
	for (const auto& n : m_graph.nodes)
		if (auto* bands = dynamic_cast<LayerBandsNode*>(n.get()))
			for (const LayerSegment& s : bands->segments)
				if (s.layer >= 1
					&& std::find(layers.begin(), layers.end(), s.layer) == layers.end())
					layers.push_back(s.layer);

	std::vector<OutputView> next;
	for (int layer : layers) {
		OutputView ov;
		ov.layer  = layer;
		ov.pixels.assign((size_t)N * N * 4, 0);
		for (const OutputView& prev : m_outputs)
			if (prev.layer == layer) { ov.tex = prev.tex; break; }
		if (!ov.tex)
			ov.tex = Texture2D::create(N, N);
		next.push_back(std::move(ov));
	}

	const RulePlan plan = buildRulePlan(m_graph);
	if (plan.valid()) {
		// Preview scale: height in [0,1] so a Height attribute's thresholds are
		// authored in the same units the preview shows.
		RuleEvalContext ctx;
		ctx.heightGrid  = m_heightField.data();
		ctx.heightRes   = N;
		ctx.tileSize    = 1.0f;
		ctx.heightScale = 1.0f;
		ctx.worldOrigin = glm::vec2(0.0f);

		RulePlanScratch scratch;
		for (int z = 0; z < N; ++z) {
			for (int x = 0; x < N; ++x) {
				RuleEvalContext c = ctx;
				c.sample = glm::vec2((float)x, (float)z);
				float w[kMaxMaterialLayers - 1];
				plan.evaluate(scratch, c, w, kMaxMaterialLayers - 1);

				for (OutputView& ov : next) {
					const int ch = ov.layer - 1;
					const float v = (ch >= 0 && ch < kMaxMaterialLayers - 1)
						? std::clamp(w[ch], 0.0f, 1.0f) : 0.0f;
					const uint8_t g = (uint8_t)std::lround(v * 255.0f);
					uint8_t* p = &ov.pixels[((size_t)z * N + x) * 4];
					p[0] = p[1] = p[2] = g;
					p[3] = 255;
				}
			}
		}
	}

	for (OutputView& ov : next)
		ov.tex->setData(ov.pixels.data(), (uint32_t)ov.pixels.size());

	m_outputs = std::move(next);
	m_graphHash = hashGraph(m_graph);
}

}
