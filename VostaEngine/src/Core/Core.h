#ifndef CORE_H
#define CORE_H

#include <memory>

#if defined(_WIN32)
	#define VE_PLATFORM_WINDOWS 1
#elif defined(__APPLE__)
	#include <TargetConditionals.h>
	#if TARGET_OS_IPHONE
		#define VE_PLATFORM_IOS 1
	#else
		#define VE_PLATFORM_MACOS 1
	#endif
#elif defined(__linux__)
	#define VE_PLATFORM_LINUX 1
#else
	#error "Vosta Engine: unsupported platform"
#endif

// VE_API marks engine symbols the editor consumes: export when building the
// engine DLL, import when consuming it. VE_EXPORT is for a shared library that
// is not built as the engine (a game module's C entry points), so it always
// exports and never depends on VE_BUILD_DLL.
#if defined(VE_PLATFORM_WINDOWS)
	#ifdef VE_BUILD_DLL
		#define VE_API __declspec(dllexport)
	#else
		#define VE_API __declspec(dllimport)
	#endif // VE_BUILD_DLL
	#define VE_EXPORT __declspec(dllexport)
#else
	#define VE_API __attribute__((visibility("default")))
	#define VE_EXPORT __attribute__((visibility("default")))
#endif

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
