#include "vepch.h"
#include "Stack.h"

namespace ve::vellum {

	void Stack::add(const Ref<Widget>& child, const glm::vec2& offset, const glm::vec2& anchor) {
		addChild(child);
		m_offsets.push_back(offset);
		m_anchors.push_back(anchor);
	}

	glm::vec2 Stack::computeDesiredSize() const {
		// Grows to the furthest child corner; the stack itself has no size of its own.
		glm::vec2 extent{ 0.0f };
		for (std::size_t i = 0; i < m_children.size(); ++i)
			extent = glm::max(extent, offsetFor(i) + m_children[i]->desiredSize());
		return extent;
	}

	void Stack::arrange(const Rect& slot) {
		m_rect = slot;
		for (std::size_t i = 0; i < m_children.size(); ++i) {
			// Children keep the size they measured; only the origin moves. The anchor
			// slides the child across the leftover space, then the offset nudges it.
			const glm::vec2 size = m_children[i]->desiredSize();
			const glm::vec2 pos = slot.pos + (slot.size - size) * anchorFor(i) + offsetFor(i);
			m_children[i]->arrange({ pos, size });
		}
	}

	glm::vec2 Stack::offsetFor(std::size_t index) const {
		return index < m_offsets.size() ? m_offsets[index] : glm::vec2(0.0f);
	}

	glm::vec2 Stack::anchorFor(std::size_t index) const {
		return index < m_anchors.size() ? m_anchors[index] : glm::vec2(0.0f);
	}

}
