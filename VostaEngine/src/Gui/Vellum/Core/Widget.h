#pragma once

#include "Core/Core.h"
#include "Layout.h"
#include "Reply.h"

#include <cstdint>
#include <string>

namespace ve::vellum {

// Defined alongside the paint backend. Widgets only emit commands into it.
// Declared `class` (not `struct`) to match PaintContext.h: MSVC encodes the
// tag in the mangled name, so a struct/class mismatch here silently produces a
// different symbol for paint() and breaks the link.
class PaintContext;

// Owns mouse routing over the tree; sets the hovered/pressed flags below.
class WidgetTree;

// Base node of the retained widget tree.
// Layout is the Slate model: a bottom-up prepass caches every widget's intrinsic
// desired size, then an arrange pass hands out the final absolute rects top-down.
class VE_API Widget {
public:
	virtual ~Widget();

	// Bottom-up layout pass: resolves this widget's desired size from its children
	// and caches it. Cheap to call every frame — a valid cache short-circuits the
	// whole subtree. `layoutScale` is the logical-to-physical factor the tree is
	// being prepared under (1 when the design is drawn unscaled); text needs it to
	// rasterise at the size it will actually occupy on screen.
	void prepass(float layoutScale = 1.0f);

	// The factor this widget was last prepared under.
	float layoutScale() const { return m_layoutScale; }

	// Arrange top-down: the parent decides the slot, the widget stores it as its
	// final absolute rect.
	virtual void arrange(const Rect& slot) = 0;

	// Default recurses children; leaves override to draw themselves.
	virtual void paint(PaintContext& ctx);

	// Per-frame animation step, in seconds. The tree drives it after routing input
	// and before painting, so a state change made this frame animates this frame.
	// The default recurses children; an animating widget overrides it and calls
	// Widget::tick(dt) to keep its descendants ticking.
	virtual void tick(float dt);

	// Cached intrinsic size. Valid after prepass(); containers read it on their
	// children while arranging.
	glm::vec2 desiredSize() const;

	void addChild(const Ref<Widget>& child);

	// Author name. Unique within one authored tree; runtime code binds events to
	// widgets through it, so it is the tree's stable lookup key.
	void setName(const std::string& name) { m_name = name; }
	const std::string& name() const { return m_name; }

	// Text bubble WidgetTree shows after the cursor rests on this widget. Empty
	// disables it; the deepest widget with text on the hover path wins.
	void setTooltip(const std::string& text) { m_tooltip = text; }
	const std::string& tooltip() const { return m_tooltip; }

	Widget* parent() const { return m_parent; }
	const std::vector<Ref<Widget>>& children() const { return m_children; }
	Rect bounds() const { return m_rect; }

	// True when `p` (absolute screen space) lies in this widget's final rect.
	bool containsPoint(const glm::vec2& p) const;

	// Hit-testing switch, independent of paint visibility. A container that hides
	// its child by skipping paint (HudGroup) still has that child in the routed
	// tree, so the child would keep swallowing hover and clicks; turning this off
	// removes it from the hover path without dropping its layout.
	void setHitTestVisible(bool visible) { m_hitTestVisible = visible; }
	bool isHitTestVisible() const { return m_hitTestVisible; }

	// Router-owned interaction state; widgets read it to pick their visual state.
	bool isHovered() const { return m_hovered; }
	bool isPressed() const { return m_pressed; }
	bool isFocused() const { return m_focused; }

	// Mouse handlers. Default demands nothing, so the event bubbles to the parent.
	// Override to react — return Reply to claim the event and optionally the capture.
	virtual Reply onMouseDown(const glm::vec2& pos);
	virtual Reply onMouseUp(const glm::vec2& pos);
	virtual Reply onMouseMove(const glm::vec2& pos);

	virtual Reply onMouseWheel(float delta);
	virtual bool canConsumeWheel() const;

	// Keyboard handlers, delivered only to the focused widget. Return true when
	// consumed. Same convention as the mouse handlers: most widgets ignore them.
	virtual bool onKeyPressed(int key, int repeat);
	virtual bool onTextInput(uint32_t codepoint);

	// Focus transitions; not called for a repeated focus request on the same widget.
	virtual void onFocus();
	virtual void onBlur();

protected:
	// Intrinsic size derived from the children's desiredSize(). Runs after the
	// children were prepped and before any arrange(), so it must not read m_rect.
	virtual glm::vec2 computeDesiredSize() const = 0;

	// Drops the cached size on this widget and every ancestor.
	void invalidateDesiredSize();

	std::string              m_name;              // authored lookup key
	std::string              m_tooltip;           // shown by the tree while hovered
	Widget*                  m_parent = nullptr;  // non-owning: children own, parent only observes
	std::vector<Ref<Widget>> m_children;          // owning
	Rect                     m_rect{};            // absolute screen space, set by arrange()
	bool                     m_hovered = false;   // set by the router
	bool                     m_pressed = false;   // set while this widget holds capture
	bool                     m_focused = false;   // set while this widget owns keyboard focus
	bool                     m_hitTestVisible = true;  // false keeps it out of the hover path

private:
	friend class WidgetTree;

	glm::vec2 m_desiredSize{ 0.0f };              // cache, filled by prepass()
	bool      m_desiredSizeValid = false;
	float     m_layoutScale = 1.0f;               // logical -> physical, from prepass()
};

}
