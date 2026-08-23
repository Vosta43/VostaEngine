#ifndef EVENT_H
#define EVENT_H

#include <string>
#include <chrono>

namespace ve {

#define EVENT_CLASS_TYPE(type) \
		static Type getStaticType() { return Type::type; } \
		virtual Type getType() const override { return getStaticType(); } \
		virtual const char* getName() const override { return #type; }

#define EVENT_CLASS_CATEGORY(category) \
		virtual int getCategoryFlags() const override { return category; }

#define BIT(x) (1 << (x))

	class Event {
	public:
		enum class Type {
			None = 0,
			WindowClose, WindowResize, WindowFocus, WindowLostFocus, WindowMoved,
			KeyPressed, KeyReleased, KeyTyped,
			MouseMoved, MouseScrolled, MouseButtonPressed, MouseButtonReleased,
			AppTick, AppUpdate, AppRender,
			Custom
		};

		enum class Category {
			None = 0,
			Application = BIT(0),
			Input = BIT(1),
			Keyboard = BIT(2),
			Mouse = BIT(3),
			MouseButton = BIT(4),
			Window = BIT(5)
		};

	protected:
		Event() : m_handled(false), m_timestamp(getCurrentTime()) {}

	public:
		virtual ~Event() = default;

		// Core virtual functions
		virtual Type getType() const = 0;
		virtual const char* getName() const = 0;
		virtual int getCategoryFlags() const = 0;
		virtual std::string toString() const { return getName(); }

		bool isInCategory(Category category) const {
			return getCategoryFlags() & static_cast<int>(category);
		}

		bool isHandled() const { return m_handled; }
		void setHandled(bool handled = true) { m_handled = handled; }

		float getTimestamp() const { return m_timestamp; }

	private:
		static float getCurrentTime() {
			static auto start = std::chrono::high_resolution_clock::now();
			auto now = std::chrono::high_resolution_clock::now();
			return std::chrono::duration<float>(now - start).count();
		}

	private:
		bool m_handled;
		float m_timestamp;
	};

}

#endif