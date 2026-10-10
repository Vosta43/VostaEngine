#include "vepch.h"
#include "WidgetTree.h"

namespace ve::vellum {

    void WidgetTree::setRoot(const Ref<Widget>& root) {
        // The router keeps raw pointers into the old tree; clear them before the
        // tree it points at can go away.
        if (m_hoverTarget) m_hoverTarget->m_hovered = false;
        if (m_captureTarget) m_captureTarget->m_pressed = false;
        m_hoverTarget = nullptr;
        m_captureTarget = nullptr;
        m_path.clear();
        m_root = root;
    }

    void WidgetTree::frame(const glm::vec2& surfaceSize, const InputFrame& input) {
        if (!m_root || surfaceSize.x <= 0.0f || surfaceSize.y <= 0.0f) return;

        // Two-pass layout: prepass bottom-up to cache desired sizes, then arrange
        // top-down so every widget ends up with its final absolute rect. The root
        // is pinned to the whole surface, matching how a view constrains its child.
        m_root->prepass();
        m_root->arrange({ { 0.0f, 0.0f }, surfaceSize });

        // Rects are final now, so hit-testing is valid; route before painting so
        // the state this frame's widgets read is already up to date.
        applyInput(input);

        m_paint.beginFrame(surfaceSize);
        m_root->paint(m_paint);

        m_renderer.render(m_paint.list(), surfaceSize);
    }

    void WidgetTree::buildPath(const glm::vec2& pos) {
        m_path.clear();

        Widget* w = m_root.get();
        if (!w || !w->containsPoint(pos))
            return;

        m_path.push_back(w);
        for (;;) {
            // Topmost child wins: later children paint on top, so scan in reverse.
            Widget* hitChild = nullptr;
            const auto& kids = w->children();
            for (auto it = kids.rbegin(); it != kids.rend(); ++it) {
                if ((*it)->containsPoint(pos)) {
                    hitChild = (*it).get();
                    break;
                }
            }
            if (!hitChild)
                break;
            m_path.push_back(hitChild);
            w = hitChild;
        }
    }

    void WidgetTree::updateHover() {
        Widget* target = m_path.empty() ? nullptr : m_path.back();
        if (target == m_hoverTarget)
            return;
        if (m_hoverTarget) m_hoverTarget->m_hovered = false;
        m_hoverTarget = target;
        if (m_hoverTarget) m_hoverTarget->m_hovered = true;
    }

    void WidgetTree::applyInput(const InputFrame& input) {
        if (m_captureTarget) {
            // A capture owns the pointer: hover elsewhere is frozen, and the
            // release is delivered here even if the cursor left the widget.
            // Release also when the button reads up without an edge, so a missed
            // event can never leave the capture stuck.
            if (input.justUp || !input.buttonDown) {
                m_captureTarget->onMouseUp(input.pos);
                m_captureTarget->m_pressed = false;
                m_captureTarget = nullptr;
            }
            else {
                m_captureTarget->onMouseMove(input.pos);
            }
            return;
        }

        buildPath(input.pos);
        updateHover();

        if (input.justDown && !m_path.empty()) {
            // Bubble from the deepest widget outward; first handler wins.
            for (auto it = m_path.rbegin(); it != m_path.rend(); ++it) {
                Reply reply = (*it)->onMouseDown(input.pos);
                if (!reply.handled)
                    continue;
                if (reply.capture) {
                    m_captureTarget = *it;
                    m_captureTarget->m_pressed = true;
                }
                break;
            }
        }
    }

}
