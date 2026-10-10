#include "vepch.h"
#include "Widget.h"

namespace ve::vellum {

Widget::~Widget() = default;

void Widget::prepass() {
	if (m_desiredSizeValid)
		return;
	for (auto& child : m_children)
		child->prepass();
	m_desiredSize = computeDesiredSize();
	m_desiredSizeValid = true;
}

glm::vec2 Widget::desiredSize() const {
	VE_CORE_ASSERT(m_desiredSizeValid, "desiredSize() read before prepass()");
	return m_desiredSize;
}

void Widget::invalidateDesiredSize() {
	// Walking to the root keeps every ancestor cache honest: a stale parent would
	// otherwise short-circuit its own prepass and never pick up the new child size.
	for (Widget* w = this; w != nullptr; w = w->m_parent)
		w->m_desiredSizeValid = false;
}

void Widget::paint(PaintContext& ctx) {
	for (auto& child : m_children)
		child->paint(ctx);
}

void Widget::addChild(const Ref<Widget>& child) {
	if (!child)
		return;
	child->m_parent = this;
	m_children.push_back(child);
	invalidateDesiredSize();
}

bool Widget::containsPoint(const glm::vec2& p) const {
	return contains(m_rect, p);
}

Reply Widget::onMouseDown(const glm::vec2&) {
	return Reply::unhandled();
}

Reply Widget::onMouseUp(const glm::vec2&) {
	return Reply::unhandled();
}

Reply Widget::onMouseMove(const glm::vec2&) {
	return Reply::unhandled();
}

}
