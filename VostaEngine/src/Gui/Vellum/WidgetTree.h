#pragma once

#include "Core/Core.h"
#include "Core/DrawList.h"
#include "Core/InputFrame.h"
#include "Core/Layout.h"
#include "Core/PaintContext.h"
#include "Core/Widget.h"
#include "VellumRenderer.h"
#include "Widgets/TooltipBox.h"

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

        // Moves keyboard focus. Null blurs. A mouse down that does not request
        // focus through its Reply clears the current target.
        void setFocus(Widget* target);
        Widget* focusTarget() const { return m_focusTarget; }

        // Lay out, route `input`, and submit one frame for a surface of
        // `surfaceSize` pixels.
        void frame(const glm::vec2& surfaceSize, const InputFrame& input);

        // Same, with widgets laid out in logical units and the surface showing
        // `layout.logicalSize()` of them. The tree is arranged against that logical
        // size, the mouse is mapped into it, and the renderer maps back out — so a
        // resize changes only the transform, never the tree.
        void frame(const glm::vec2& surfaceSize, const LayoutTransform& layout,
                   const InputFrame& input);

        // A widget painted after the root and its tooltip, laid out against the
        // whole logical size. It is not part of the routed tree, so it survives a
        // root swap and never takes the pointer. Null clears it.
        void setOverlay(const Ref<Widget>& overlay) { m_overlay = overlay; }
        Widget* overlay() const { return m_overlay.get(); }

        bool wantsWheel(const glm::vec2& pos) const;

        // Tooltip support. The tree owns the bubble; leaving the font null keeps
        // tooltips disabled. `seconds` is how long the cursor must rest first.
        void setTooltipFont(FontAtlas* font);
        void setTooltipDelay(float seconds);
        float tooltipDelay() const { return m_tooltipDelay; }

        const DrawList& drawList() const { return m_paint.list(); }
        VellumRenderer& renderer() { return m_renderer; }

    private:
        // Collects root -> deepest widget containing `pos` into m_path.
        void buildPath(const glm::vec2& pos);
        static void collectPath(Widget* root, const glm::vec2& pos, std::vector<Widget*>& out);
        // Drops router state pointing at widgets no longer under the root. Lists
        // rebuild their rows between frames, so a captured or hovered row can be
        // destroyed while the pointer is still down.
        void dropStaleRouterState();
        bool isInTree(const Widget* widget) const;
        void dispatchWheel(float delta);
        // Drives hover/capture/focus from this frame's input and dispatches edges.
        void applyInput(const InputFrame& input);
        // Points the hover flag at the deepest hit widget (or nothing).
        void updateHover();
        // Feeds this frame's key events to the focus target, in arrival order.
        void dispatchKeys(const InputFrame& input);
        // Picks the hover path's deepest tooltip text and runs the reveal timer.
        void updateTooltip(const InputFrame& input);
        void paintTooltip();
        void paintOverlay(float dt);
        Widget* tooltipSource() const;

        Ref<Widget>    m_root;
        Ref<Widget>    m_overlay;                      // painted last, never routed
        PaintContext   m_paint;
        VellumRenderer m_renderer;

        std::vector<Widget*> m_path;               // root -> hit leaf, rebuilt per frame
        Widget*              m_hoverTarget = nullptr;   // non-owning
        Widget*              m_captureTarget = nullptr; // non-owning, until button release
        Widget*              m_focusTarget = nullptr;   // non-owning, until blurred

        // Tooltip state. The bubble lives outside the routed tree, so it has no
        // hover/press flags of its own and cannot intercept the pointer.
        Ref<TooltipBox> m_tooltipBox;
        Widget*         m_tooltipSource = nullptr;      // non-owning: widget being described
        float           m_tooltipRevealTime = 0.0f;     // host clock value that reveals it
        float           m_tooltipDelay = 0.5f;
        bool            m_tooltipVisible = false;
        glm::vec2       m_tooltipAnchor{ 0.0f };        // pinned at reveal, not followed
        glm::vec2       m_logicalSize{ 0.0f };          // design units the root is arranged against
        LayoutTransform m_layout{};                     // logical -> surface, this frame
        float           m_lastTimeSeconds = 0.0f;       // previous frame's host clock
    };

}
