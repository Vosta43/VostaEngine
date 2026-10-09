#include "NoiseEditorSchema.h"

#include "Noise/NoiseNodes.h"

#include <imgui_node_editor.h>

#include <algorithm>
#include <vector>

namespace ve {

namespace {

	// Side of a Noise Unit's inline thumbnail, and the label column width shared by
	// every row of the settings form.
	constexpr int   kThumbSize   = 141;   // 64 * 2.2, the inline preview's grown side
	constexpr float kItemWidth   = 170.0f;

	// FNV-1a over the raw settings bytes: changes exactly when a parameter does, which
	// is what re-bakes the unit's thumbnail.
	uint32_t hashSettings(const NoiseSettings& s) {
		const uint8_t* p = reinterpret_cast<const uint8_t*>(&s);
		uint32_t h = 2166136261u;
		for (size_t i = 0; i < sizeof(NoiseSettings); ++i) {
			h ^= p[i];
			h *= 16777619u;
		}
		return h;
	}

	// Section label for the form. ImGui::SeparatorText sizes its rule — and the
	// item rect it registers — to the window's WorkRect, which inside a node is the
	// whole canvas: the rule streaks to the right edge and the oversized item makes
	// every strip along it report a hovered item, which blocks the canvas context
	// menu that node creation needs. Draw a rule bounded by the item width instead.
	void sectionHeader(const char* label) {
		ImGui::Spacing();
		ImGui::TextDisabled("%s", label);
		const ImVec2 p = ImGui::GetCursorScreenPos();
		const float  w = ImGui::CalcItemWidth();
		ImGui::GetWindowDrawList()->AddLine(
			ImVec2(p.x, p.y + 1.0f), ImVec2(p.x + w, p.y + 1.0f),
			ImGui::GetColorU32(ImGuiCol_Separator));
		ImGui::Dummy(ImVec2(w, 2.0f));
	}

}

ImColor NoiseEditorSchema::getPinColor(PinType type) const {
	(void)type;   // noise graphs carry scalars only
	return ImColor(190, 190, 190);
}

ImColor NoiseEditorSchema::getNodeColor(NodeType type) const {
	switch (type) {
		case NodeType::Input:   return ImColor(70, 140, 70, 200);
		case NodeType::Math:    return ImColor(60, 60, 200, 200);
		case NodeType::Utility: return ImColor(140, 80, 160, 200);
		case NodeType::Output:  return ImColor(128, 128, 128, 200);
		default:                return ImColor(128, 128, 128, 200);
	}
}

bool NoiseEditorSchema::canDeleteNode(const GraphNode& node) const {
	// Every noise graph needs its Output terminal.
	return dynamic_cast<const NoiseOutputNode*>(&node) == nullptr;
}

void NoiseEditorSchema::onNodeDeleted(uint32_t nodeId) {
	m_thumbs.erase(nodeId);
	m_collapsed.erase(nodeId);
}

void NoiseEditorSchema::resetNodeState() {
	m_thumbs.clear();
	m_collapsed.clear();
}

void NoiseEditorSchema::drawUnitPreview(uint32_t nodeId, NoiseSettings& s) {
	NodeThumb& thumb = m_thumbs[nodeId];
	const uint32_t h = hashSettings(s);

	if (!thumb.tex)
		thumb.tex = Texture2D::create(kThumbSize, kThumbSize);

	if (thumb.hash != h) {
		thumb.hash = h;

		// Previewed in the unit's own declared range — the same remap the bake applies
		// to that unit before the graph's math combines it.
		const float lo    = s.outputMin;
		const float range = s.outputMax - s.outputMin;
		const float half  = (float)kThumbSize * 0.5f;

		std::vector<uint8_t> px((size_t)kThumbSize * kThumbSize * 4);
		for (int y = 0; y < kThumbSize; ++y) {
			for (int x = 0; x < kThumbSize; ++x) {
				const float v = NoiseCPU::sample(s, (float)x - half, (float)y - half);
				const float t = range > 1e-6f ? (v - lo) / range : 0.5f;
				const uint8_t g = (uint8_t)std::clamp(t * 255.0f + 0.5f, 0.0f, 255.0f);

				uint8_t* p = &px[((size_t)y * kThumbSize + x) * 4];
				p[0] = p[1] = p[2] = g;
				p[3] = 255;
			}
		}
		thumb.tex->setData(px.data(), (uint32_t)px.size());
	}

	ImGui::Image((void*)(intptr_t)thumb.tex->getRendererID(), ImVec2((float)kThumbSize, (float)kThumbSize));
}

// A dropdown cannot be opened from inside a node: imgui-node-editor draws nodes in a
// panned/zoomed local space while ImGui popups are plain screen-space windows, so a
// popup created there is double-mapped — it lands in the wrong place and its clicks
// miss, leaking to the canvas (which clears the selection). This button only shows the
// current value and latches the request; drawSuspendedContent builds the list outside
// the canvas transform, where popups behave.
void NoiseEditorSchema::enumDropdown(const char* label, const char* const* names, int count,
	int current, std::function<void(int)> onPick, bool canvasSpace)
{
	ImGui::PushID(label);

	if (ImGui::Button(names[current], ImVec2(kItemWidth, 0.0f))) {
		const ImVec2 rect = ImGui::GetItemRectMin();
		m_dropdown.requestOpen = true;
		m_dropdown.anchor      = canvasSpace ? ax::NodeEditor::CanvasToScreen(rect) : rect;
		m_dropdown.names       = names;
		m_dropdown.count       = count;
		m_dropdown.current     = current;
		m_dropdown.onPick      = onPick;
	}

	// Down caret, so the row reads as a dropdown rather than a plain button.
	{
		const ImVec2 mn   = ImGui::GetItemRectMin();
		const ImVec2 mx   = ImGui::GetItemRectMax();
		const float  half = ImGui::GetFrameHeight() * 0.16f;
		const float  cx   = mx.x - ImGui::GetStyle().FramePadding.x - half;
		const float  cy   = (mn.y + mx.y) * 0.5f;
		ImGui::GetWindowDrawList()->AddTriangleFilled(
			ImVec2(cx - half, cy - half * 0.6f),
			ImVec2(cx + half, cy - half * 0.6f),
			ImVec2(cx, cy + half * 0.8f),
			ImGui::GetColorU32(ImGuiCol_Text));
	}

	ImGui::PopID();
	ImGui::SameLine();
	ImGui::Text("%s", label);
}

void NoiseEditorSchema::drawNoiseSettings(NoiseSettings& s, bool canvasSpace) {
	using S = NoiseSettings;

	static const char* kTypeNames[] = { "OpenSimplex2", "OpenSimplex2S", "Cellular", "Perlin", "ValueCubic", "Value" };
	static const char* kFractalNames[] = { "None", "FBm", "Ridged", "PingPong" };
	static const char* kCellularDistNames[] = { "Euclidean", "EuclideanSq", "Manhattan", "Hybrid" };
	static const char* kCellularReturnNames[] = { "CellValue", "Distance", "Distance2", "Distance2Add", "Distance2Sub", "Distance2Mul", "Distance2Div" };
	static const char* kDomainWarpNames[] = { "None", "OpenSimplex2", "OpenSimplex2Reduced", "BasicGrid" };

	ImGui::PushItemWidth(kItemWidth);

	enumDropdown("Type", kTypeNames, IM_ARRAYSIZE(kTypeNames), static_cast<int>(s.type),
		[&s](int i) { s.type = static_cast<S::Type>(i); }, canvasSpace);
	ImGui::DragInt("Seed", &s.seed);
	ImGui::DragFloat("Frequency", &s.frequency, 0.0005f, 0.0001f, 0.1f, "%.4f");

	sectionHeader("Fractal");
	enumDropdown("Fractal", kFractalNames, IM_ARRAYSIZE(kFractalNames), static_cast<int>(s.fractal),
		[&s](int i) { s.fractal = static_cast<S::Fractal>(i); }, canvasSpace);
	if (s.fractal != S::Fractal::None) {
		ImGui::DragInt("Octaves", &s.octaves, 0.15f, 1, 12);
		ImGui::DragFloat("Lacunarity", &s.lacunarity, 0.01f, 0.0f, 8.0f, "%.2f");
		ImGui::DragFloat("Gain", &s.gain, 0.005f, 0.0f, 1.0f, "%.3f");
		if (s.fractal == S::Fractal::Fbm || s.fractal == S::Fractal::Ridged)
			ImGui::DragFloat("Weighted Strength", &s.weightedStrength, 0.005f, 0.0f, 1.0f, "%.3f");
		if (s.fractal == S::Fractal::PingPong)
			ImGui::DragFloat("Ping-Pong Strength", &s.pingPongStrength, 0.05f, 0.0f, 10.0f, "%.2f");
	}

	if (s.type == S::Type::Cellular) {
		sectionHeader("Cellular");
		enumDropdown("Distance Fn", kCellularDistNames, IM_ARRAYSIZE(kCellularDistNames), static_cast<int>(s.cellularDistance),
			[&s](int i) { s.cellularDistance = static_cast<S::CellularDistance>(i); }, canvasSpace);
		enumDropdown("Return Type", kCellularReturnNames, IM_ARRAYSIZE(kCellularReturnNames), static_cast<int>(s.cellularReturn),
			[&s](int i) { s.cellularReturn = static_cast<S::CellularReturn>(i); }, canvasSpace);
		ImGui::DragFloat("Jitter", &s.cellularJitter, 0.005f, 0.0f, 1.0f, "%.3f");
	}

	sectionHeader("Domain Warp");
	enumDropdown("Warp", kDomainWarpNames, IM_ARRAYSIZE(kDomainWarpNames), static_cast<int>(s.domainWarp),
		[&s](int i) { s.domainWarp = static_cast<S::DomainWarp>(i); }, canvasSpace);
	if (s.domainWarp != S::DomainWarp::None)
		ImGui::DragFloat("Warp Amp", &s.domainWarpAmp, 0.5f, 0.0f, 100.0f, "%.1f");

	sectionHeader("Output");
	ImGui::DragFloatRange2("Range", &s.outputMin, &s.outputMax, 0.01f, -10.0f, 10.0f, "%.2f", "%.2f");

	ImGui::PopItemWidth();
}

bool NoiseEditorSchema::drawParams(GraphNode* node, bool canvasSpace) {
	(void)canvasSpace;

	// The unit's own form already manages its item width.
	if (auto* unit = dynamic_cast<NoiseUnitNode*>(node)) {
		drawNoiseSettings(unit->settings, canvasSpace);
		return true;
	}

	ImGui::PushItemWidth(kItemWidth);

	if (auto* c = dynamic_cast<NoiseConstantNode*>(node)) {
		ImGui::DragFloat("Value", &c->value, 0.005f, 0.0f, 0.0f, "%.3f");
	} else if (auto* r = dynamic_cast<NoiseRemapNode*>(node)) {
		ImGui::DragFloat("In Min",  &r->inMin,  0.01f, 0.0f, 0.0f, "%.2f");
		ImGui::DragFloat("In Max",  &r->inMax,  0.01f, 0.0f, 0.0f, "%.2f");
		ImGui::DragFloat("Out Min", &r->outMin, 0.01f, 0.0f, 0.0f, "%.2f");
		ImGui::DragFloat("Out Max", &r->outMax, 0.01f, 0.0f, 0.0f, "%.2f");
		ImGui::Checkbox("Clamp", &r->clamp);
	} else if (auto* cl = dynamic_cast<NoiseClampNode*>(node)) {
		ImGui::DragFloat("Min", &cl->minValue, 0.005f, 0.0f, 0.0f, "%.3f");
		ImGui::DragFloat("Max", &cl->maxValue, 0.005f, 0.0f, 0.0f, "%.3f");
	} else if (auto* th = dynamic_cast<NoiseThresholdNode*>(node)) {
		ImGui::DragFloat("Threshold", &th->threshold, 0.005f, 0.0f, 0.0f, "%.3f");
		ImGui::DragFloat("Falloff",   &th->falloff,   0.002f, 0.0f, 1.0f, "%.3f");
	} else if (auto* e = dynamic_cast<NoiseErfNode*>(node)) {
		ImGui::DragFloat("Divisor", &e->divisor, 0.001f, 0.001f, 1.0f, "%.4f");
	} else if (auto* sp = dynamic_cast<NoiseSplineNode*>(node)) {
		drawSplineEditor(sp);
	} else if (auto* out = dynamic_cast<NoiseOutputNode*>(node)) {
		ImGui::DragFloatRange2("Range", &out->outputMin, &out->outputMax, 0.01f, -10.0f, 10.0f, "%.2f", "%.2f");
	} else {
		// Pure operators (Add/Multiply/Min/Max/Invert/Blend) carry no parameters.
		ImGui::PopItemWidth();
		return false;
	}

	ImGui::PopItemWidth();
	return true;
}

void NoiseEditorSchema::drawSplineEditor(NoiseSplineNode* sp) {
	// A curve with fewer than two points has no slope to draw; fall back to a ramp.
	if (sp->points.size() < 2)
		sp->points = { glm::vec2(0.0f, 0.0f), glm::vec2(1.0f, 1.0f) };

	const ImVec2 size(168.0f, 110.0f);
	const ImVec2 p0 = ImGui::GetCursorScreenPos();
	const ImVec2 p1(p0.x + size.x, p0.y + size.y);

	ImGui::InvisibleButton("##spline", size);
	const bool active  = ImGui::IsItemActive();
	const bool hovered = ImGui::IsItemHovered();

	SplineDrag& drag = m_splineDrags[ImGui::GetItemID()];

	// Freeze the display domain while a point is being dragged, so moving the point
	// cannot shift the value<->pixel mapping out from under the cursor.
	if (!(active && drag.point >= 0)) {
		float xmin = sp->points.front().x, xmax = sp->points.back().x;
		if (xmax - xmin < 1e-4f) xmax = xmin + 1.0f;

		float ymin = sp->points.front().y, ymax = sp->points.front().y;
		for (const auto& q : sp->points) {
			ymin = std::min(ymin, q.y);
			ymax = std::max(ymax, q.y);
		}
		if (ymax - ymin < 1e-4f) { ymin -= 0.5f; ymax += 0.5f; }

		const float pad = (ymax - ymin) * 0.08f;
		drag.dx = ImVec2(xmin, xmax);
		drag.dy = ImVec2(ymin - pad, ymax + pad);
	}
	const ImVec2 dx = drag.dx;
	const ImVec2 dy = drag.dy;

	auto toScreen = [&](const glm::vec2& v) {
		return ImVec2(p0.x + (v.x - dx.x) / (dx.y - dx.x) * size.x,
		              p1.y - (v.y - dy.x) / (dy.y - dy.x) * size.y);
	};
	auto toValue = [&](const ImVec2& m) {
		return glm::vec2(dx.x + (m.x - p0.x) / size.x * (dx.y - dx.x),
		                 dy.x + (p1.y - m.y) / size.y * (dy.y - dy.x));
	};

	ImDrawList* dl = ImGui::GetWindowDrawList();
	dl->AddRectFilled(p0, p1, IM_COL32(22, 22, 26, 255), 3.0f);
	dl->AddLine(ImVec2(p0.x, (p0.y + p1.y) * 0.5f), ImVec2(p1.x, (p0.y + p1.y) * 0.5f),
		IM_COL32(255, 255, 255, 20));

	// Sample through evaluate() itself, so the drawn line is exactly the mapping the
	// graph will apply.
	std::vector<float> in{ 0.0f };
	ImVec2 prev(0.0f, 0.0f);
	const int steps = 120;
	for (int i = 0; i <= steps; ++i) {
		const float x = dx.x + (dx.y - dx.x) * (float)i / (float)steps;
		in[0] = x;
		const ImVec2 s = toScreen(glm::vec2(x, sp->evaluate(in, 0.0f, 0.0f)));
		if (i > 0) dl->AddLine(prev, s, IM_COL32(120, 210, 255, 255), 1.6f);
		prev = s;
	}

	const ImVec2 mouse = ImGui::GetMousePos();
	int hoverPt = -1;
	for (size_t i = 0; i < sp->points.size(); ++i) {
		const ImVec2 s = toScreen(sp->points[i]);
		const float ddx = mouse.x - s.x, ddy = mouse.y - s.y;
		const bool hot = (ddx * ddx + ddy * ddy) < 49.0f;   // 7px grab radius
		if (hot) hoverPt = (int)i;

		dl->AddCircleFilled(s, hot ? 5.0f : 4.0f, IM_COL32(255, 200, 60, 255));
		dl->AddCircle(s, hot ? 5.0f : 4.0f, IM_COL32(0, 0, 0, 255), 0, 1.2f);
	}

	if (ImGui::IsItemActivated())
		drag.point = hoverPt;   // -1 when the press missed every point

	if (active && drag.point >= 0 && drag.point < (int)sp->points.size()) {
		const int i = drag.point;
		glm::vec2 v = toValue(mouse);
		// Keep points sorted by x without reindexing mid-drag.
		const float eps = (dx.y - dx.x) * 1e-3f;
		if (i + 1 < (int)sp->points.size()) v.x = std::min(v.x, sp->points[i + 1].x - eps);
		if (i > 0)                          v.x = std::max(v.x, sp->points[i - 1].x + eps);
		sp->points[i] = v;
	}

	if (ImGui::IsItemDeactivated())
		drag.point = -1;

	if (hovered && hoverPt >= 0 && ImGui::IsMouseClicked(1) && sp->points.size() > 2) {
		sp->points.erase(sp->points.begin() + hoverPt);
		drag.point = -1;
	} else if (hovered && hoverPt < 0 && ImGui::IsMouseDoubleClicked(0)) {
		const glm::vec2 v = toValue(mouse);
		auto it = sp->points.begin();
		while (it != sp->points.end() && it->x < v.x) ++it;
		sp->points.insert(it, v);
	}
}

void NoiseEditorSchema::drawNodeContent(GraphNode* node) {
	if (auto* unit = dynamic_cast<NoiseUnitNode*>(node)) {
		drawUnitPreview(unit->id, unit->settings);

		auto it = m_collapsed.find(unit->id);
		bool collapsed = (it == m_collapsed.end()) || it->second;

		if (ImGui::ArrowButton("##settings", collapsed ? ImGuiDir_Right : ImGuiDir_Down)) {
			collapsed = !collapsed;
			m_collapsed[unit->id] = collapsed;
		}
		ImGui::SameLine();
		ImGui::TextUnformatted("Settings");

		if (!collapsed)
			drawParams(node, true);
	} else {
		drawParams(node, true);
	}
}

void NoiseEditorSchema::drawDetailsContent(GraphNode* node) {
	ImGui::TextUnformatted(node->m_displayName.c_str());
	ImGui::Separator();

	if (!drawParams(node, false))
		ImGui::TextDisabled("No parameters");
}

void NoiseEditorSchema::drawSuspendedContent(NodeGraph& graph) {
	(void)graph;

	if (m_dropdown.requestOpen) {
		ImGui::SetCursorScreenPos(m_dropdown.anchor);
		ImGui::OpenPopup("##noiseEnum");
		m_dropdown.requestOpen = false;
	}

	if (ImGui::BeginPopup("##noiseEnum")) {
		for (int i = 0; i < m_dropdown.count && m_dropdown.names; ++i) {
			if (ImGui::Selectable(m_dropdown.names[i], i == m_dropdown.current)) {
				if (m_dropdown.onPick)
					m_dropdown.onPick(i);
				m_dropdown.names = nullptr;
				m_dropdown.onPick = nullptr;
				ImGui::CloseCurrentPopup();
			}
		}
		ImGui::EndPopup();
	} else if (m_dropdown.names) {
		// Dismissed without a pick: drop the pending edit.
		m_dropdown.names = nullptr;
		m_dropdown.onPick = nullptr;
	}
}

void NoiseEditorSchema::buildCreateMenu(NodeGraph& graph, const glm::vec2& canvasPos,
	const std::function<void(Ref<GraphNode>, const glm::vec2&)>& addNode) {
	(void)graph;
	if (ImGui::MenuItem("Noise Unit"))
		addNode(CreateRef<NoiseUnitNode>(), canvasPos);
	if (ImGui::MenuItem("Constant"))
		addNode(CreateRef<NoiseConstantNode>(), canvasPos);

	ImGui::Separator();
	if (ImGui::MenuItem("Add"))
		addNode(CreateRef<NoiseAddNode>(), canvasPos);
	if (ImGui::MenuItem("Subtract"))
		addNode(CreateRef<NoiseSubtractNode>(), canvasPos);
	if (ImGui::MenuItem("Multiply"))
		addNode(CreateRef<NoiseMultiplyNode>(), canvasPos);
	if (ImGui::MenuItem("Divide"))
		addNode(CreateRef<NoiseDivideNode>(), canvasPos);
	if (ImGui::MenuItem("Min"))
		addNode(CreateRef<NoiseMinNode>(), canvasPos);
	if (ImGui::MenuItem("Max"))
		addNode(CreateRef<NoiseMaxNode>(), canvasPos);
	if (ImGui::MenuItem("Abs"))
		addNode(CreateRef<NoiseAbsNode>(), canvasPos);

	ImGui::Separator();
	if (ImGui::MenuItem("Blend"))
		addNode(CreateRef<NoiseBlendNode>(), canvasPos);
	if (ImGui::MenuItem("Remap"))
		addNode(CreateRef<NoiseRemapNode>(), canvasPos);
	if (ImGui::MenuItem("Threshold"))
		addNode(CreateRef<NoiseThresholdNode>(), canvasPos);
	if (ImGui::MenuItem("Invert"))
		addNode(CreateRef<NoiseInvertNode>(), canvasPos);
	if (ImGui::MenuItem("Clamp"))
		addNode(CreateRef<NoiseClampNode>(), canvasPos);
	if (ImGui::MenuItem("Erf"))
		addNode(CreateRef<NoiseErfNode>(), canvasPos);
	if (ImGui::MenuItem("Trunc"))
		addNode(CreateRef<NoiseTruncNode>(), canvasPos);
	if (ImGui::MenuItem("Spline"))
		addNode(CreateRef<NoiseSplineNode>(), canvasPos);
}

}
