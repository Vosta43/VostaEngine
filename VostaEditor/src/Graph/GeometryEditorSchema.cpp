#include "GeometryEditorSchema.h"

#include "Asset/AssetLibrary.h"
#include "Geometry/GeometryNodes.h"
#include "Sdf/SdfNodes.h"
#include "Noise/NoiseSettings.h"

#include <imgui.h>
#include <imgui_node_editor.h>

#include <algorithm>
#include <filesystem>

namespace ve {

namespace {

	constexpr float kItemWidth = 180.0f;

	// Bounded section rule. ImGui::SeparatorText sizes its rule — and the item rect it
	// registers — to the window WorkRect, which inside a node is the whole canvas, so
	// it streaks to the right edge and its oversized item blocks the canvas context
	// menu. Mirror NoiseEditorSchema's fix.
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

ImColor GeometryEditorSchema::getPinColor(PinType type) const {
	switch (type) {
		case PinType::Geometry: return ImColor(150, 90, 200);
		case PinType::Sdf:      return ImColor(90, 200, 200);
		case PinType::Float:    return ImColor(200, 80, 80);
		case PinType::Float2:   return ImColor(80, 180, 120);
		case PinType::Float3:   return ImColor(80, 120, 220);
		case PinType::Float4:   return ImColor(200, 180, 80);
		default:                return ImColor(190, 190, 190);
	}
}

ImColor GeometryEditorSchema::getNodeColor(NodeType type) const {
	switch (type) {
		case NodeType::Input:   return ImColor(70, 140, 70, 200);
		case NodeType::Math:    return ImColor(60, 60, 200, 200);
		case NodeType::Utility: return ImColor(140, 80, 160, 200);
		case NodeType::Output:  return ImColor(128, 128, 128, 200);
		default:                return ImColor(128, 128, 128, 200);
	}
}

bool GeometryEditorSchema::canDeleteNode(const GraphNode& node) const {
	// Every geometry graph needs its Mesh Output terminal.
	return dynamic_cast<const MeshOutputNode*>(&node) == nullptr;
}

void GeometryEditorSchema::enumDropdown(const char* label, const char* const* names, int count,
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

void GeometryEditorSchema::assetPicker(const char* label, const std::string& currentPath, bool canvasSpace,
	std::function<void(const std::string&)> onPick)
{
	ImGui::PushID(label);

	const std::string shown = currentPath.empty()
		? std::string("None")
		: std::filesystem::path(currentPath).stem().string();

	if (ImGui::Button(shown.c_str(), ImVec2(kItemWidth, 0.0f))) {
		const ImVec2 rect = ImGui::GetItemRectMin();
		m_assetPicker.requestOpen = true;
		m_assetPicker.anchor      = canvasSpace ? ax::NodeEditor::CanvasToScreen(rect) : rect;
		m_assetPicker.onPick      = std::move(onPick);

		// Snapshot the project's noise graphs now: the deferred popup draws a frame
		// later, outside the canvas, and must not walk the library again then.
		m_assetPicker.items.clear();
		AssetLibrary::get().refresh();
		for (const AssetEntry& entry : AssetLibrary::get().list())
			if (entry.typeKey == "noise")
				m_assetPicker.items.push_back(entry.path);
	}

	ImGui::PopID();
	ImGui::SameLine();
	ImGui::Text("%s", label);
}

void GeometryEditorSchema::drawNoiseSettings(NoiseSettings& s, bool canvasSpace) {
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
	// Logarithmic + wide range: unit-space sampling (the displace node) reaches a
	// whole sphere in ~2 units, so usable frequencies sit around 1-10, while the
	// terrain-scale assets this field also serves live near 0.005-0.1.
	ImGui::DragFloat("Frequency", &s.frequency, 0.0f, 0.0001f, 10.0f, "%.4f", ImGuiSliderFlags_Logarithmic);

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

bool GeometryEditorSchema::drawParams(GraphNode* node, bool canvasSpace) {
	// The displace node's form manages its own item width via drawNoiseSettings.
	if (auto* s = dynamic_cast<IcosphereNode*>(node)) {
		ImGui::PushItemWidth(kItemWidth);
		ImGui::DragFloat("Radius", &s->radius, 0.02f, 0.01f, 100.0f, "%.2f");
		ImGui::DragInt("Subdivisions", &s->subdivisions, 0.1f, 0, 6);
		ImGui::DragFloat3("Scale", &s->scale.x, 0.02f, 0.01f, 10.0f, "%.2f");
		ImGui::PopItemWidth();
		return true;
	}

	if (auto* sd = dynamic_cast<SubdivideNode*>(node)) {
		ImGui::PushItemWidth(kItemWidth);
		ImGui::DragInt("Iterations", &sd->iterations, 0.1f, 0, 4);
		ImGui::Checkbox("Smooth (unit sphere)", &sd->smooth);
		ImGui::PopItemWidth();
		return true;
	}

	if (auto* d = dynamic_cast<NoiseDisplaceNode*>(node)) {
		using D = NoiseDisplaceNode;
		static const char* kModeNames[] = { "Normal", "Direction", "XYZ" };

		ImGui::PushItemWidth(kItemWidth);
		ImGui::DragFloat("Amplitude", &d->amplitude, 0.005f, 0.0f, 10.0f, "%.3f");
		ImGui::DragFloat("Bias", &d->bias, 0.005f, -10.0f, 10.0f, "%.3f");
		enumDropdown("Direction", kModeNames, IM_ARRAYSIZE(kModeNames), static_cast<int>(d->mode),
			[d](int i) { d->mode = static_cast<D::Mode>(i); }, canvasSpace);
		if (d->mode == D::Mode::Direction)
			ImGui::DragFloat3("Vector", &d->direction.x, 0.02f, -1.0f, 1.0f, "%.2f");
		ImGui::Checkbox("Sample in unit space", &d->sampleInUnitSpace);
		ImGui::PopItemWidth();

		ImGui::Separator();
		drawNoiseSettings(d->settings, canvasSpace);
		return true;
	}

	if (auto* f = dynamic_cast<FieldDisplaceNode*>(node)) {
		using D = FieldDisplaceNode;
		static const char* kModeNames[] = { "Normal", "Direction", "XYZ" };

		ImGui::PushItemWidth(kItemWidth);
		assetPicker("Field", f->noisePath, canvasSpace,
			[f](const std::string& p) { f->noisePath = p; });
		ImGui::DragFloat("Amplitude", &f->amplitude, 0.005f, 0.0f, 10.0f, "%.3f");
		ImGui::DragFloat("Bias", &f->bias, 0.005f, -10.0f, 10.0f, "%.3f");
		enumDropdown("Direction", kModeNames, IM_ARRAYSIZE(kModeNames), static_cast<int>(f->mode),
			[f](int i) { f->mode = static_cast<D::Mode>(i); }, canvasSpace);
		if (f->mode == D::Mode::Direction)
			ImGui::DragFloat3("Vector", &f->direction.x, 0.02f, -1.0f, 1.0f, "%.2f");
		ImGui::Checkbox("Sample in unit space", &f->sampleInUnitSpace);
		ImGui::PopItemWidth();
		return true;
	}

	if (auto* s = dynamic_cast<SdfSphereNode*>(node)) {
		ImGui::PushItemWidth(kItemWidth);
		ImGui::DragFloat3("Center", &s->center.x, 0.02f, -10.0f, 10.0f, "%.2f");
		ImGui::DragFloat("Radius", &s->radius, 0.02f, 0.01f, 100.0f, "%.2f");
		ImGui::PopItemWidth();
		return true;
	}

	if (auto* nd = dynamic_cast<SdfNoiseDisplaceNode*>(node)) {
		ImGui::PushItemWidth(kItemWidth);
		assetPicker("Field", nd->noisePath, canvasSpace,
			[nd](const std::string& p) { nd->noisePath = p; });
		ImGui::DragFloat("Amplitude", &nd->amplitude, 0.005f, 0.0f, 1.0f, "%.3f");
		ImGui::DragFloat("Bias", &nd->bias, 0.005f, -1.0f, 1.0f, "%.3f");
		ImGui::DragFloat("Frequency", &nd->frequency, 0.05f, 0.01f, 32.0f, "%.2f");
		ImGui::PopItemWidth();
		return true;
	}

	if (auto* bn = dynamic_cast<SdfBooleanNode*>(node)) {
		using B = SdfBooleanNode;
		static const char* kOpNames[] = { "Union", "Subtract", "Intersect" };
		ImGui::PushItemWidth(kItemWidth);
		enumDropdown("Op", kOpNames, IM_ARRAYSIZE(kOpNames), static_cast<int>(bn->mode),
			[bn](int i) { bn->mode = static_cast<B::Mode>(i); }, canvasSpace);
		ImGui::DragFloat("Smooth", &bn->smoothRadius, 0.002f, 0.0f, 1.0f, "%.3f");
		ImGui::PopItemWidth();
		return true;
	}

	if (auto* mc = dynamic_cast<MarchingCubesNode*>(node)) {
		ImGui::PushItemWidth(kItemWidth);
		ImGui::DragInt("Resolution", &mc->resolution, 0.5f, 2, 128);
		ImGui::DragFloat3("Bounds Min", &mc->boundsMin.x, 0.02f, -100.0f, 100.0f, "%.2f");
		ImGui::DragFloat3("Bounds Max", &mc->boundsMax.x, 0.02f, -100.0f, 100.0f, "%.2f");
		ImGui::PopItemWidth();
		return true;
	}

	if (auto* t = dynamic_cast<TransformNode*>(node)) {
		ImGui::PushItemWidth(kItemWidth);
		ImGui::DragFloat3("Translate", &t->translate.x, 0.02f, 0.0f, 0.0f, "%.2f");
		ImGui::DragFloat3("Rotate", &t->rotateEulerDeg.x, 0.2f, 0.0f, 0.0f, "%.1f");
		ImGui::DragFloat3("Scale", &t->scale.x, 0.02f, 0.01f, 10.0f, "%.2f");
		ImGui::PopItemWidth();
		return true;
	}

	if (auto* o = dynamic_cast<MeshOutputNode*>(node)) {
		ImGui::PushItemWidth(kItemWidth);
		ImGui::Checkbox("Recalc normals", &o->recalcNormals);
		ImGui::Checkbox("Generate UVs", &o->generateUVs);
		ImGui::Checkbox("Recalc tangents", &o->recalcTangents);
		ImGui::PopItemWidth();
		return true;
	}

	return false;
}

void GeometryEditorSchema::drawNodeContent(GraphNode* node) {
	drawParams(node, true);
}

void GeometryEditorSchema::drawDetailsContent(GraphNode* node) {
	ImGui::TextUnformatted(node->m_displayName.c_str());
	ImGui::Separator();

	if (!drawParams(node, false))
		ImGui::TextDisabled("No parameters");
}

void GeometryEditorSchema::drawSuspendedContent(NodeGraph& graph) {
	(void)graph;

	if (m_dropdown.requestOpen) {
		ImGui::SetCursorScreenPos(m_dropdown.anchor);
		ImGui::OpenPopup("##geometryEnum");
		m_dropdown.requestOpen = false;
	}

	if (ImGui::BeginPopup("##geometryEnum")) {
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
		m_dropdown.names = nullptr;
		m_dropdown.onPick = nullptr;
	}

	if (m_assetPicker.requestOpen) {
		ImGui::SetCursorScreenPos(m_assetPicker.anchor);
		ImGui::OpenPopup("##geometryAsset");
		m_assetPicker.requestOpen = false;
		m_assetPicker.open = true;
	}

	if (ImGui::BeginPopup("##geometryAsset")) {
		if (ImGui::Selectable("None", false)) {
			if (m_assetPicker.onPick) m_assetPicker.onPick(std::string());
			m_assetPicker.onPick = nullptr;
			m_assetPicker.open = false;
			ImGui::CloseCurrentPopup();
		}
		for (const std::string& item : m_assetPicker.items) {
			if (ImGui::Selectable(item.c_str(), false)) {
				if (m_assetPicker.onPick) m_assetPicker.onPick(item);
				m_assetPicker.onPick = nullptr;
				m_assetPicker.open = false;
				ImGui::CloseCurrentPopup();
			}
		}
		ImGui::EndPopup();
	} else if (m_assetPicker.open) {
		m_assetPicker.open = false;
		m_assetPicker.onPick = nullptr;
	}
}

void GeometryEditorSchema::buildCreateMenu(NodeGraph& graph, const glm::vec2& canvasPos,
	const std::function<void(Ref<GraphNode>, const glm::vec2&)>& addNode) {
	(void)graph;
	if (ImGui::MenuItem("Icosphere"))
		addNode(CreateRef<IcosphereNode>(), canvasPos);
	if (ImGui::MenuItem("Subdivide"))
		addNode(CreateRef<SubdivideNode>(), canvasPos);
	if (ImGui::MenuItem("Noise Displace"))
		addNode(CreateRef<NoiseDisplaceNode>(), canvasPos);
	if (ImGui::MenuItem("Field Displace"))
		addNode(CreateRef<FieldDisplaceNode>(), canvasPos);

	ImGui::Separator();
	if (ImGui::MenuItem("SDF Sphere"))
		addNode(CreateRef<SdfSphereNode>(), canvasPos);
	if (ImGui::MenuItem("SDF Noise Displace"))
		addNode(CreateRef<SdfNoiseDisplaceNode>(), canvasPos);
	if (ImGui::MenuItem("SDF Boolean"))
		addNode(CreateRef<SdfBooleanNode>(), canvasPos);
	if (ImGui::MenuItem("Marching Cubes"))
		addNode(CreateRef<MarchingCubesNode>(), canvasPos);

	ImGui::Separator();
	if (ImGui::MenuItem("Transform"))
		addNode(CreateRef<TransformNode>(), canvasPos);

	ImGui::Separator();
	if (ImGui::MenuItem("Mesh Output"))
		addNode(CreateRef<MeshOutputNode>(), canvasPos);
}

}
