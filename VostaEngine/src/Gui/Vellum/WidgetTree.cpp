#include "vepch.h"
#include "WidgetTree.h"

namespace ve::vellum {

    void WidgetTree::setRoot(const Ref<Widget>& root) {
        // The router keeps raw pointers into the old tree; clear them before the
        // tree it points at can go away.
        if (m_hoverTarget) m_hoverTarget->m_hovered = false;
        if (m_captureTarget) m_captureTarget->m_pressed = false;
        if (m_focusTarget) m_focusTarget->m_focused = false;
        m_hoverTarget = nullptr;
        m_captureTarget = nullptr;
        m_focusTarget = nullptr;
        m_path.clear();
        m_root = root;
    }

    void WidgetTree::setFocus(Widget* target) {
        if (target == m_focusTarget)
            return;
        if (m_focusTarget) {
            m_focusTarget->m_focused = false;
            m_focusTarget->onBlur();
        }
        m_focusTarget = target;
        if (m_focusTarget) {
            m_focusTarget->m_focused = true;
            m_focusTarget->onFocus();
        }
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
        collectPath(m_root.get(), pos, m_path);
    }

    void WidgetTree::collectPath(Widget* root, const glm::vec2& pos,
                                 std::vector<Widget*>& out) {
        Widget* w = root;
        if (!w || !w->containsPoint(pos))
            return;

        out.push_back(w);
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
            out.push_back(hitChild);
            w = hitChild;
        }
    }

    bool WidgetTree::wantsWheel(const glm::vec2& pos) const {
        std::vector<Widget*> path;
        collectPath(m_root.get(), pos, path);
        for (Widget* widget : path)
            if (widget->canConsumeWheel())
                return true;
        return false;
    }

    void WidgetTree::dispatchWheel(float delta) {
        for (auto it = m_path.rbegin(); it != m_path.rend(); ++it) {
            if ((*it)->onMouseWheel(delta).handled)
                break;
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
            if (input.wheel != 0.0f) {
                buildPath(input.pos);
                dispatchWheel(input.wheel);
            }

            if (input.justUp || !input.buttonDown) {
                m_captureTarget->onMouseUp(input.pos);
                m_captureTarget->m_pressed = false;
                m_captureTarget = nullptr;
            }
            else {
                m_captureTarget->onMouseMove(input.pos);
            }
        }
        else {
            buildPath(input.pos);
            updateHover();

            if (input.justDown) {
                // Mouse first: a click that lands on an input still receives this
                // frame's later key events.
                Widget* focusRequest = nullptr;
                if (!m_path.empty()) {
                    // Bubble from the deepest widget outward; first handler wins.
                    for (auto it = m_path.rbegin(); it != m_path.rend(); ++it) {
                        Reply reply = (*it)->onMouseDown(input.pos);
                        if (!reply.handled)
                            continue;
                        if (reply.capture) {
                            m_captureTarget = *it;
                            m_captureTarget->m_pressed = true;
                        }
                        if (reply.focus)
                            focusRequest = *it;
                        break;
                    }
                }
                // Clicking anything that does not ask for focus (or empty space)
                // blurs the current target — the desktop text-field convention.
                setFocus(focusRequest);
            }

            if (input.wheel != 0.0f)
                dispatchWheel(input.wheel);
        }

        // Keyboard is independent of mouse capture: a drag does not steal keys.
        dispatchKeys(input);
    }

    void WidgetTree::dispatchKeys(const InputFrame& input) {
        for (const KeyInput& key : input.keys) {
            if (!m_focusTarget)
                break;
            if (key.kind == KeyInput::Kind::Down)
                m_focusTarget->onKeyPressed(key.key, key.repeat);
            else
                m_focusTarget->onTextInput(key.codepoint);
        }
    }

}
