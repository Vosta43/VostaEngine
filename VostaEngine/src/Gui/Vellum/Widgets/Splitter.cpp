#include "vepch.h"
#include "Splitter.h"
#include "Gui/Vellum/Core/PaintContext.h"

namespace ve::vellum {

	class Splitter::Handle : public Widget {
	public:
		Handle(Splitter& owner, int boundary)
			: m_owner(owner), m_boundary(boundary) {}

		glm::vec2 computeDesiredSize() const override {
			return m_owner.m_orientation == Orientation::Horizontal
				? glm::vec2(m_owner.m_handleThickness, 0.0f)
				: glm::vec2(0.0f, m_owner.m_handleThickness);
		}

		void arrange(const Rect& slot) override { m_rect = slot; }

		// The owner reaches the protected invalidateDesiredSize() through this.
		void refreshDesiredSize() { invalidateDesiredSize(); }

		void onPaint(PaintContext& ctx) override {
			const glm::vec4& color = isPressed() ? m_owner.m_handlePressedColor
				: isHovered() ? m_owner.m_handleHoverColor
				: m_owner.m_handleColor;
			ctx.fillRect(m_rect, color);
		}

		Reply onMouseDown(const glm::vec2& pos) override {
			m_owner.beginDrag(m_boundary, pos);
			return Reply::handleAndCapture();
		}

		Reply onMouseMove(const glm::vec2& pos) override {
			m_owner.dragTo(pos);
			return Reply::handle();
		}

		Reply onMouseUp(const glm::vec2&) override {
			m_owner.endDrag();
			return Reply::handle();
		}

	private:
		Splitter& m_owner;
		int       m_boundary;
	};

	Splitter::Splitter(Orientation orientation)
		: m_orientation(orientation) {}

	void Splitter::setOrientation(Orientation orientation) {
		if (orientation == m_orientation)
			return;
		m_orientation = orientation;
		for (auto& handle : m_handles)
			handle->refreshDesiredSize();
		invalidateDesiredSize();
	}

	void Splitter::addSlot(const Ref<Widget>& content, float weight, float minSize) {
		if (!content)
			return;

		if (!m_slots.empty()) {
			Ref<Handle> handle = CreateRef<Handle>(*this, static_cast<int>(m_slots.size()) - 1);
			addChild(handle);
			m_handles.push_back(handle);
		}
		addChild(content);
		m_slots.push_back({ content, weight, minSize });
	}

	void Splitter::setHandleThickness(float pixels) {
		if (pixels == m_handleThickness)
			return;
		m_handleThickness = pixels;
		for (auto& handle : m_handles)
			handle->refreshDesiredSize();
		invalidateDesiredSize();
	}

	glm::vec2 Splitter::computeDesiredSize() const {
		const bool horizontal = m_orientation == Orientation::Horizontal;

		float along = 0.0f;
		float across = 0.0f;
		for (const Slot& slot : m_slots) {
			const glm::vec2 child = slot.content->desiredSize();
			along += horizontal ? child.x : child.y;
			across = glm::max(across, horizontal ? child.y : child.x);
		}
		along += m_handleThickness * static_cast<float>(m_handles.size());

		return horizontal ? glm::vec2(along, across) : glm::vec2(across, along);
	}

	void Splitter::arrange(const Rect& slot) {
		m_rect = slot;

		const std::size_t count = m_slots.size();
		if (count == 0)
			return;

		const bool horizontal = m_orientation == Orientation::Horizontal;
		const float total = horizontal ? slot.size.x : slot.size.y;
		const float handleSpace = m_handleThickness * static_cast<float>(count - 1);
		const float contentSpace = glm::max(0.0f, total - handleSpace);

		std::vector<float> weights(count, 1.0f);
		float weightSum = 0.0f;
		for (std::size_t i = 0; i < count; ++i) {
			weights[i] = glm::max(m_slots[i].weight, 0.0f);
			weightSum += weights[i];
		}
		// All-zero weights fall back to an even split instead of an empty layout.
		if (weightSum <= 0.0f) {
			weightSum = static_cast<float>(count);
			std::fill(weights.begin(), weights.end(), 1.0f);
		}

		// Water-fill: weights decide the sizes, then slots that came out below their
		// minimum are pinned there and the rest is redistributed over the others.
		std::vector<float> sizes(count, 0.0f);
		std::vector<bool> pinned(count, false);
		for (std::size_t pass = 0; pass <= count; ++pass) {
			float free = contentSpace;
			float freeWeight = 0.0f;
			for (std::size_t i = 0; i < count; ++i) {
				if (pinned[i])
					free -= sizes[i];
				else
					freeWeight += weights[i];
			}
			if (freeWeight <= 0.0f) {
				for (std::size_t i = 0; i < count; ++i)
					if (!pinned[i])
						sizes[i] = 0.0f;
				break;
			}

			bool repinned = false;
			for (std::size_t i = 0; i < count; ++i) {
				if (pinned[i])
					continue;
				sizes[i] = free * weights[i] / freeWeight;
				if (sizes[i] < m_slots[i].minSize) {
					sizes[i] = m_slots[i].minSize;
					pinned[i] = true;
					repinned = true;
				}
			}
			if (!repinned)
				break;
		}

		float cursor = horizontal ? slot.pos.x : slot.pos.y;
		for (std::size_t i = 0; i < count; ++i) {
			m_slots[i].content->arrange(horizontal
				? Rect{ { cursor, slot.pos.y }, { sizes[i], slot.size.y } }
				: Rect{ { slot.pos.x, cursor }, { slot.size.x, sizes[i] } });
			cursor += sizes[i];

			if (i < m_handles.size()) {
				m_handles[i]->arrange(horizontal
					? Rect{ { cursor, slot.pos.y }, { m_handleThickness, slot.size.y } }
					: Rect{ { slot.pos.x, cursor }, { slot.size.x, m_handleThickness } });
				cursor += m_handleThickness;
			}
		}
	}

	void Splitter::onPaint(PaintContext& ctx) {
		// Slots are clipped to the splitter so an over-constrained layout (minimums
		// summing past the available space) cannot bleed over its neighbours.
		ctx.pushClip(m_rect);
		Widget::onPaint(ctx);
		ctx.popClip();
	}

	float Splitter::axisOf(const glm::vec2& p) const {
		return m_orientation == Orientation::Horizontal ? p.x : p.y;
	}

	float Splitter::slotSize(int index) const {
		const Rect rect = m_slots[static_cast<std::size_t>(index)].content->bounds();
		return m_orientation == Orientation::Horizontal ? rect.size.x : rect.size.y;
	}

	void Splitter::beginDrag(int boundary, const glm::vec2& pos) {
		if (boundary < 0 || boundary + 1 >= static_cast<int>(m_slots.size()))
			return;

		m_dragBoundary = boundary;
		m_dragStartAxis = axisOf(pos);
		m_dragStartSizeA = slotSize(boundary);
		m_dragPairSize = m_dragStartSizeA + slotSize(boundary + 1);
		m_dragPairWeight = m_slots[boundary].weight + m_slots[boundary + 1].weight;
		m_dragMinA = m_slots[boundary].minSize;
		m_dragMinB = m_slots[boundary + 1].minSize;
	}

	void Splitter::dragTo(const glm::vec2& pos) {
		if (m_dragBoundary < 0 || m_dragPairWeight <= 0.0f || m_dragPairSize <= 0.0f)
			return;

		// Pixels per weight unit stays as it was at drag start, so the pair keeps its
		// combined weight and the drag cannot disturb slots further down the axis.
		float sizeA = m_dragStartSizeA + (axisOf(pos) - m_dragStartAxis);
		const float upper = m_dragPairSize - m_dragMinB;
		if (m_dragMinA <= upper)
			sizeA = glm::clamp(sizeA, m_dragMinA, upper);
		else
			sizeA = glm::min(sizeA, m_dragPairSize);

		const float pxPerWeight = m_dragPairSize / m_dragPairWeight;
		m_slots[m_dragBoundary].weight = sizeA / pxPerWeight;
		m_slots[m_dragBoundary + 1].weight =
			m_dragPairWeight - m_slots[m_dragBoundary].weight;

		// Apply now: arrange() already ran for this frame, so waiting would make the
		// handle lag a frame behind the cursor.
		arrange(Rect{ m_rect.pos, m_rect.size });
	}

	void Splitter::endDrag() {
		m_dragBoundary = -1;
	}

}
