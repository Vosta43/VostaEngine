#pragma once

#include "Core/Core.h"
#include "Core/DrawList.h"
#include "Core/InputFrame.h"
#include "Core/Layout.h"
#include "Core/PaintContext.h"
#include "Core/Widget.h"
#include "VellumRenderer.h"

#include <vector>

namespace ve::vellum {

    // Drives one UI frame: lays the tree out, routes mouse input, paints into a
    // DrawList, submits it. The widgets themselves never see the surface size or
    // the renderer — those live here, mirroring Slate's FSlateApplication and
    // Flutter's RenderView. It also owns the router state (hover target and mouse
    // capture), the same role FSlateApplication plays for Slate.
    class VE_API WidgetTree {
    public:
        void setRoot(const Ref<Widget>& root);
        Widget* root() const { return m_root.get(); }

        // Lay out, route `input`, and submit one frame for a surface of
        // `surfaceSize` pixels.
        void frame(const glm::vec2& surfaceSize, const InputFrame& input);

        const DrawList& drawList() const { return m_paint.list(); }
        VellumRenderer& renderer() { return m_renderer; }

    private:
        // Collects root -> deepest widget containing `pos` into m_path.
        void buildPath(const glm::vec2& pos);
        // Drives hover/capture from this frame's input and dispatches edges.
        void applyInput(const InputFrame& input);
        // Points the hover flag at the deepest hit widget (or nothing).
        void updateHover();

        Ref<Widget>    m_root;
        PaintContext   m_paint;
        VellumRenderer m_renderer;

        std::vector<Widget*> m_path;               // root -> hit leaf, rebuilt per frame
        Widget*              m_hoverTarget = nullptr;   // non-owning
        Widget*              m_captureTarget = nullptr; // non-owning, until button release
    };

}
