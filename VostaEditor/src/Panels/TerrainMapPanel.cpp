#include "TerrainMapPanel.h"

#include "Scene/Scene.h"
#include "Scene/Components.h"
#include "Scene/Terrain/Terrain.h"

#include "imgui.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <unordered_set>

namespace ve {

namespace {

    // Packed grid coordinate. The high half is x, the low half y, so a cell keys
    // a hash set unambiguously across negative coordinates.
    int64_t packCoord(const glm::ivec2& coord) {
        return ((int64_t)coord.x << 32) ^ (int64_t)(uint32_t)coord.y;
    }

    constexpr float kMinZoom = 4.0f;
    constexpr float kMaxZoom = 96.0f;
    // Below this a cell is too small for grid lines to read as a lattice.
    constexpr float kGridLineZoom = 6.0f;

    const ImU32 kBackground = IM_COL32(24, 24, 28, 255);
    const ImU32 kGridLine   = IM_COL32(52, 52, 60, 255);
    const ImU32 kAxisLine   = IM_COL32(96, 96, 110, 255);
    const ImU32 kTileFill   = IM_COL32(235, 235, 235, 255);
    const ImU32 kSelFill    = IM_COL32(90, 150, 255, 60);
    const ImU32 kSelLine    = IM_COL32(120, 175, 255, 255);

    const char* plural(int n) { return n == 1 ? "" : "s"; }

}

void TerrainMapPanel::onGuiRender(Scene& scene, bool* openFlag) {

    ImGui::SetNextWindowSize(ImVec2(760.0f, 600.0f), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Terrain Map", openFlag)) {
        ImGui::End();
        return;
    }

    // Occupancy snapshot: a value set of coordinates, so nothing below holds a
    // component pointer and a generate/delete mid-frame cannot dangle one.
    std::unordered_set<int64_t> occupied;
    for (uint32_t id : scene.getRegistry().view<TerrainComponent>())
        occupied.insert(packCoord(scene.getComponent<TerrainComponent>(id).tileCoord));

    ImGui::Text("Tiles: %d", (int)occupied.size());
    ImGui::SameLine();
    ImGui::TextDisabled("| wheel zoom, middle-drag pan, left-drag select, right-click menu");
    ImGui::Separator();

    const ImVec2 canvasPos = ImGui::GetCursorScreenPos();
    ImVec2 canvasSize = ImGui::GetContentRegionAvail();
    canvasSize.x = std::max(canvasSize.x, 64.0f);
    canvasSize.y = std::max(canvasSize.y, 64.0f);
    const ImVec2 canvasEnd(canvasPos.x + canvasSize.x, canvasPos.y + canvasSize.y);

    ImGui::InvisibleButton("##tvcanvas", canvasSize,
        ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonRight |
        ImGuiButtonFlags_MouseButtonMiddle);
    const bool hovered = ImGui::IsItemHovered();
    const bool active = ImGui::IsItemActive();

    ImDrawList* dl = ImGui::GetWindowDrawList();
    const ImVec2 mouse = ImGui::GetIO().MousePos;

    const ImVec2 originPx(canvasPos.x + canvasSize.x * 0.5f + m_pan.x,
                          canvasPos.y + canvasSize.y * 0.5f + m_pan.y);

    auto cellAt = [&](const ImVec2& p) {
        return glm::ivec2((int)std::lround((p.x - originPx.x) / m_zoom),
                          (int)std::lround((p.y - originPx.y) / m_zoom));
    };
    auto cellCenterOf = [&](const glm::ivec2& c) {
        return ImVec2(originPx.x + c.x * m_zoom, originPx.y + c.y * m_zoom);
    };

    // --- Input ---
    if (hovered && ImGui::GetIO().MouseWheel != 0.0f) {
        // Keep the world point under the cursor fixed while zooming.
        const ImVec2 rel(mouse.x - originPx.x, mouse.y - originPx.y);
        const float newZoom = std::clamp(m_zoom * std::pow(1.1f, ImGui::GetIO().MouseWheel),
            kMinZoom, kMaxZoom);
        const float k = newZoom / m_zoom;
        m_pan.x += rel.x * (1.0f - k);
        m_pan.y += rel.y * (1.0f - k);
        m_zoom = newZoom;
    }

    if (active && ImGui::IsMouseDragging(ImGuiMouseButton_Middle)) {
        const ImVec2 d = ImGui::GetIO().MouseDelta;
        m_pan.x += d.x;
        m_pan.y += d.y;
    }

    // Left drag rubber-bands a rectangle; it stays selected after release.
    if (hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
        m_dragStart = cellAt(mouse);
        m_selMin = m_selMax = m_dragStart;
        m_hasSelection = true;
        m_dragging = true;
    }
    if (m_dragging) {
        if (ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
            const glm::ivec2 c = cellAt(mouse);
            m_selMin = glm::min(m_dragStart, c);
            m_selMax = glm::max(m_dragStart, c);
        } else {
            m_dragging = false;
        }
    }

    // Right click is the only thing that opens the menu: on the selection when the
    // click lands inside it, otherwise on the single cell under the cursor.
    if (hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
        const glm::ivec2 c = cellAt(mouse);
        const bool inside = m_hasSelection &&
            c.x >= m_selMin.x && c.x <= m_selMax.x && c.y >= m_selMin.y && c.y <= m_selMax.y;
        if (inside) {
            m_popupMin = m_selMin;
            m_popupMax = m_selMax;
        } else {
            m_popupMin = m_popupMax = c;
            m_selMin = m_selMax = c;
            m_hasSelection = true;
        }
        m_openPopup = true;
    }

    if (ImGui::IsWindowFocused() && ImGui::IsKeyPressed(ImGuiKey_Escape))
        m_hasSelection = false;

    // --- Draw ---
    dl->PushClipRect(canvasPos, canvasEnd, true);
    dl->AddRectFilled(canvasPos, canvasEnd, kBackground);

    // Visible cell range, so the lattice and the tile loop only touch what is on
    // screen however far the view has been panned.
    const int minX = (int)std::floor((canvasPos.x - originPx.x) / m_zoom - 0.5f);
    const int maxX = (int)std::ceil((canvasEnd.x - originPx.x) / m_zoom + 0.5f);
    const int minY = (int)std::floor((canvasPos.y - originPx.y) / m_zoom - 0.5f);
    const int maxY = (int)std::ceil((canvasEnd.y - originPx.y) / m_zoom + 0.5f);

    if (m_zoom >= kGridLineZoom) {
        for (int x = minX; x <= maxX; ++x) {
            const float px = originPx.x + (x - 0.5f) * m_zoom;
            dl->AddLine(ImVec2(px, canvasPos.y), ImVec2(px, canvasEnd.y),
                x == 0 ? kAxisLine : kGridLine);
        }
        for (int y = minY; y <= maxY; ++y) {
            const float py = originPx.y + (y - 0.5f) * m_zoom;
            dl->AddLine(ImVec2(canvasPos.x, py), ImVec2(canvasEnd.x, py),
                y == 0 ? kAxisLine : kGridLine);
        }
    }

    // Only occupied cells get a filled quad: filling every empty cell too would be
    // tens of thousands of quads per frame once the view is zoomed out.
    const float inset = m_zoom >= 10.0f ? 1.0f : 0.0f;
    const float half = m_zoom * 0.5f - inset;
    for (int64_t key : occupied) {
        const glm::ivec2 c((int)(key >> 32), (int)(uint32_t)key);
        if (c.x < minX || c.x > maxX || c.y < minY || c.y > maxY)
            continue;
        const ImVec2 ctr = cellCenterOf(c);
        dl->AddRectFilled(ImVec2(ctr.x - half, ctr.y - half),
            ImVec2(ctr.x + half, ctr.y + half), kTileFill);
    }

    if (m_hasSelection) {
        const ImVec2 a = cellCenterOf(m_selMin);
        const ImVec2 b = cellCenterOf(m_selMax);
        const ImVec2 p0(a.x - m_zoom * 0.5f, a.y - m_zoom * 0.5f);
        const ImVec2 p1(b.x + m_zoom * 0.5f, b.y + m_zoom * 0.5f);
        dl->AddRectFilled(p0, p1, kSelFill);
        dl->AddRect(p0, p1, kSelLine, 0.0f, 0, 2.0f);
    }
    dl->PopClipRect();

    // --- Context menu (deferred OpenPopup + BeginPopup, the repo's idiom) ---
    if (m_openPopup) {
        ImGui::OpenPopup("TerrainMapContext");
        m_openPopup = false;
    }
    if (ImGui::BeginPopup("TerrainMapContext")) {
        int emptyN = 0, filledN = 0;
        for (int y = m_popupMin.y; y <= m_popupMax.y; ++y) {
            for (int x = m_popupMin.x; x <= m_popupMax.x; ++x) {
                if (occupied.count(packCoord(glm::ivec2(x, y)))) ++filledN;
                else ++emptyN;
            }
        }

        ImGui::TextDisabled("(%d, %d) - (%d, %d)",
            m_popupMin.x, m_popupMin.y, m_popupMax.x, m_popupMax.y);
        ImGui::Separator();

        char generateLabel[64];
        std::snprintf(generateLabel, sizeof(generateLabel), "Generate %d tile%s",
            emptyN, plural(emptyN));
        if (ImGui::MenuItem(generateLabel, nullptr, false, emptyN > 0))
            Terrain::addTiles(scene, m_popupMin, m_popupMax);

        char deleteLabel[64];
        std::snprintf(deleteLabel, sizeof(deleteLabel), "Delete %d tile%s",
            filledN, plural(filledN));
        if (ImGui::MenuItem(deleteLabel, nullptr, false, filledN > 0))
            Terrain::removeTiles(scene, m_popupMin, m_popupMax);

        ImGui::Separator();
        if (ImGui::MenuItem("Clear Selection", nullptr, false, m_hasSelection))
            m_hasSelection = false;

        ImGui::EndPopup();
    }

    ImGui::End();
}

} // namespace ve
