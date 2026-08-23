#ifndef CORE_H
#define CORE_H

#include <memory>

#ifdef _WIN32
	#ifdef VE_BUILD_DLL
		#define VE_API __declspec(dllexport)
	#else
		#define VE_API __declspec(dllimport)
	#endif // VE_BUILD_DLL

#endif // VE_PLATFORM_WINDOWS

#ifdef VE_DEBUG
#define VE_ASSERT(condition, message) \
		assert(condition && message)
#define VE_CORE_ASSERT(condition, message) \
		assert(condition && message)
#else
#define VE_ASSERT(condition, message)
#define VE_CORE_ASSERT(condition, message)
#endif

// Bind a member function as an event callback.
#define VE_BIND_EVENT_FN(fn) std::bind(&fn,this,std::placeholders::_1)

namespace ve {
	// Semantic aliases for smart pointers: Scope<T> for exclusive ownership, Ref<T> for shared ownership.
	template<typename T>
	using Scope = std::unique_ptr<T>;

	template<typename T>
	using Ref = std::shared_ptr<T>;

	template<typename T, typename... Args>
	Scope<T> CreateScope(Args&&... args) {
		return std::make_unique<T>(std::forward<Args>(args)...);
	}

	template<typename T, typename... Args>
	Ref<T> CreateRef(Args&&... args) {
		return std::make_shared<T>(std::forward<Args>(args)...);
	}
}

#endif // !CORE_H
