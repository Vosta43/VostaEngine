#pragma once

#include "Core/Core.h"
#include "Gui/Vellum/Core/Widget.h"

#include <vector>

namespace ve::vellum {

	// Slate-style splitter: slots laid out along one axis with a draggable handle
	// between neighbours. Sizes are stored as relative weights, so a resized
	// splitter keeps its proportions; a drag only touches the two slots it
	// separates and keeps their combined weight constant. Each slot may declare a
	// pixel minimum.
	class VE_API Splitter : public Widget {
	public:
		enum class Orientation { Horizontal, Vertical };

		explicit Splitter(Orientation orientation = Orientation::Horizontal);

		void setOrientation(Orientation orientation);
		Orientation orientation() const { return m_orientation; }

		// `weight` is relative to the other slots; `minSize` is in pixels along the
		// split axis. Slots are added in visual order (left to right, top to bottom).
		void addSlot(const Ref<Widget>& content, float weight = 1.0f, float minSize = 0.0f);

		void setHandleThickness(float pixels);
		float handleThickness() const { return m_handleThickness; }
		void setHandleColor(const glm::vec4& color) { m_handleColor = color; }
		void setHandleHoverColor(const glm::vec4& color) { m_handleHoverColor = color; }
		void setHandlePressedColor(const glm::vec4& color) { m_handlePressedColor = color; }

		void arrange(const Rect& slot) override;
		void onPaint(PaintContext& ctx) override;

	protected:
		glm::vec2 computeDesiredSize() const override;

	private:
		// Thin draggable bar between two slots. Defined in the .cpp — it exists only
		// to take the pointer capture, so it never needs to leave this file.
		class Handle;

		struct Slot {
			Ref<Widget> content;
			float       weight = 1.0f;
			float       minSize = 0.0f;   // pixels along the split axis
		};

		// Drag state, captured when a handle takes the pointer.
		void beginDrag(int boundary, const glm::vec2& pos);
		void dragTo(const glm::vec2& pos);
		void endDrag();
		float axisOf(const glm::vec2& p) const;
		float slotSize(int index) const;

		std::vector<Slot>        m_slots;
		std::vector<Ref<Handle>> m_handles;   // one per boundary, m_handles[i] sits after slot i

		Orientation m_orientation = Orientation::Horizontal;
		float       m_handleThickness = 6.0f;
		glm::vec4   m_handleColor{ 0.17f, 0.18f, 0.21f, 1.0f };
		glm::vec4   m_handleHoverColor{ 0.32f, 0.34f, 0.39f, 1.0f };
		glm::vec4   m_handlePressedColor{ 0.46f, 0.49f, 0.55f, 1.0f };

		int   m_dragBoundary = -1;
		float m_dragStartAxis = 0.0f;
		float m_dragStartSizeA = 0.0f;
		float m_dragPairSize = 0.0f;     // combined pixel size of both slots at drag start
		float m_dragPairWeight = 0.0f;   // combined weight, held constant during the drag
		float m_dragMinA = 0.0f;
		float m_dragMinB = 0.0f;
	};

}
