#include "vepch.h"
#include "Widget.h"
#include "PaintContext.h"

#include <algorithm>

namespace ve::vellum {

Widget::~Widget() = default;

void Widget::prepass(float layoutScale) {
	const bool rescale = m_layoutScale != layoutScale;
	if (rescale) {
		// Sizes measured under the old factor are stale: text quantises its atlas
		// to whole physical pixels, so the same logical size can measure half a
		// pixel differently at another scale.
		m_layoutScale = layoutScale;
		m_desiredSizeValid = false;
	}
	if (m_desiredSizeValid)
		return;
	for (auto& child : m_children)
		child->prepass(layoutScale);
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
	if (!m_visible)
		return;

	const bool transformed = m_hasRenderTransform;
	if (transformed) {
		const glm::vec2 pivot = m_rect.pos + m_pivot01 * m_rect.size;
		ctx.pushTransform(m_translation, m_rotation, m_scale, pivot);
	}
	if (m_opacity < 1.0f)
		ctx.pushOpacity(m_opacity);

	onPaint(ctx);

	if (m_opacity < 1.0f)
		ctx.popOpacity();
	if (transformed)
		ctx.popTransform();
}

void Widget::onPaint(PaintContext& ctx) {
	for (auto& child : m_children)
		child->paint(ctx);
}

void Widget::setRenderTransform(const glm::vec2& translation, float rotationRad,
                                const glm::vec2& scale, const glm::vec2& pivot01) {
	m_translation = translation;
	m_rotation = rotationRad;
	m_scale = scale;
	m_pivot01 = pivot01;
	m_hasRenderTransform = true;
}

void Widget::clearRenderTransform() {
	m_hasRenderTransform = false;
	m_translation = glm::vec2(0.0f);
	m_rotation = 0.0f;
	m_scale = glm::vec2(1.0f);
	m_pivot01 = glm::vec2(0.0f);
}

void Widget::setRenderOpacity(float opacity) {
	m_opacity = std::clamp(opacity, 0.0f, 1.0f);
}

void Widget::tick(float dt) {
	for (auto& child : m_children)
		child->tick(dt);
}

void Widget::addChild(const Ref<Widget>& child) {
	if (!child)
		return;
	child->m_parent = this;
	m_children.push_back(child);
	invalidateDesiredSize();
}

bool Widget::containsPoint(const glm::vec2& p) const {
	return m_visible && m_hitTestVisible && contains(m_rect, p);
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

Reply Widget::onMouseWheel(float) {
	return Reply::unhandled();
}

bool Widget::canConsumeWheel() const {
	return false;
}

bool Widget::onKeyPressed(int, int) {
	return false;
}

bool Widget::onTextInput(uint32_t) {
	return false;
}

void Widget::onFocus() {}

void Widget::onBlur() {}

}
