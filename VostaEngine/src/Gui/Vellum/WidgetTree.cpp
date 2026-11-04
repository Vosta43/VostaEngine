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
        m_tooltipSource = nullptr;
        m_tooltipVisible = false;
        m_path.clear();
        m_root = root;
    }

    void WidgetTree::setTooltipFont(FontAtlas* font) {
        if (!font)
            return;
        if (!m_tooltipBox)
            m_tooltipBox = CreateRef<TooltipBox>(font);
        else
            m_tooltipBox->setFont(font);
    }

    void WidgetTree::setTooltipDelay(float seconds) {
        m_tooltipDelay = glm::max(0.0f, seconds);
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
        frame(surfaceSize, LayoutTransform{}, input);
    }

    void WidgetTree::frame(const glm::vec2& surfaceSize, const LayoutTransform& layout,
                           const InputFrame& input) {
        if (!m_root || surfaceSize.x <= 0.0f || surfaceSize.y <= 0.0f) {
            m_tooltipSource = nullptr;
            m_tooltipVisible = false;
            return;
        }

        m_layout = layout;
        m_logicalSize = layout.logicalSize(surfaceSize);
        dropStaleRouterState();

        // Animation step, taken from the host clock so a paused or backwards clock
        // yields 0 (which snaps animations to their target instead of stalling).
        // The cap keeps a long hitch from teleporting a transition.
        float dt = 0.0f;
        if (m_lastTimeSeconds > 0.0f && input.timeSeconds > m_lastTimeSeconds)
            dt = glm::min(input.timeSeconds - m_lastTimeSeconds, 0.1f);
        m_lastTimeSeconds = input.timeSeconds;

        // Two-pass layout: prepass bottom-up to cache desired sizes, then arrange
        // top-down so every widget ends up with its final absolute rect. The root
        // is pinned to the whole surface, matching how a view constrains its child.
        m_root->prepass(layout.scale);
        m_root->arrange({ { 0.0f, 0.0f }, m_logicalSize });

        // Rects are final now, so hit-testing is valid; route before painting so
        // the state this frame's widgets read is already up to date. The router and
        // every handler work in logical units, so the pointer is mapped once here.
        InputFrame local = input;
        local.pos = layout.toLogical(input.pos);
        applyInput(local);
        // After input: a hover state set above is what this frame's animation sees.
        m_root->tick(dt);
        updateTooltip(local);

        m_paint.beginFrame(m_logicalSize);
        m_root->paint(m_paint);
        paintTooltip();

        m_renderer.render(m_paint.list(), surfaceSize, layout);
    }

    bool WidgetTree::isInTree(const Widget* widget) const {
        // Compares pointers during the walk only: a stale target may already be
        // freed, so its own fields must never be read.
        std::vector<const Widget*> stack{ m_root.get() };
        while (!stack.empty()) {
            const Widget* node = stack.back();
            stack.pop_back();
            if (!node)
                continue;
            if (node == widget)
                return true;
            for (const auto& child : node->children())
                stack.push_back(child.get());
        }
        return false;
    }

    void WidgetTree::dropStaleRouterState() {
        // Flags on the dropped widgets are not cleared: the widget may be freed.
        if (m_hoverTarget && !isInTree(m_hoverTarget))
            m_hoverTarget = nullptr;
        if (m_captureTarget && !isInTree(m_captureTarget))
            m_captureTarget = nullptr;
        if (m_focusTarget && !isInTree(m_focusTarget))
            m_focusTarget = nullptr;
        if (m_tooltipSource && !isInTree(m_tooltipSource)) {
            m_tooltipSource = nullptr;
            m_tooltipVisible = false;
        }
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
        collectPath(m_root.get(), m_layout.toLogical(pos), path);
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

    Widget* WidgetTree::tooltipSource() const {
        // The deepest widget with text wins; a nested widget without a tooltip does
        // not suppress its ancestors'.
        for (auto it = m_path.rbegin(); it != m_path.rend(); ++it) {
            if (!(*it)->tooltip().empty())
                return *it;
        }
        return nullptr;
    }

    void WidgetTree::updateTooltip(const InputFrame& input) {
        if (!m_tooltipBox) {
            m_tooltipVisible = false;
            return;
        }

        // A press or an active drag dismisses the bubble and restarts the timer.
        Widget* source = (m_captureTarget || input.justDown) ? nullptr : tooltipSource();
        if (source != m_tooltipSource) {
            m_tooltipSource = source;
            m_tooltipRevealTime = input.timeSeconds + m_tooltipDelay;
            m_tooltipVisible = false;
        }
        if (!source)
            return;

        if (!m_tooltipVisible && input.timeSeconds >= m_tooltipRevealTime) {
            m_tooltipVisible = true;
            m_tooltipAnchor = input.pos;   // pinned here: the bubble does not follow
        }
        if (m_tooltipVisible) {
            m_tooltipBox->setText(source->tooltip());
            m_tooltipBox->setAnchor(m_tooltipAnchor);
        }
    }

    void WidgetTree::paintTooltip() {
        if (!m_tooltipVisible || !m_tooltipBox)
            return;
        // Laid out against the whole surface and painted last, once the root's clip
        // stack has unwound, so a panel the widget sits in cannot cut it off.
        m_tooltipBox->prepass(m_layout.scale);
        m_tooltipBox->arrange({ { 0.0f, 0.0f }, m_logicalSize });
        m_tooltipBox->paint(m_paint);
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
