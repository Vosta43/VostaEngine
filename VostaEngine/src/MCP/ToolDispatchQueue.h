#pragma once

#include "Core/Core.h"

#include <deque>
#include <future>
#include <mutex>
#include <string>

namespace ve {

	// The transport that carries a tool call onto the render thread. Tool handlers
	// touch the Scene and GL, so they must run there; anything else (a future HTTP
	// worker, a background thread) reaches them through this queue. Kept separate
	// from ToolRegistry on purpose: the registry is the table, this is only how you
	// get to it from another thread.
	//
	// Carries strings, not parsed arguments. The JsonReader holding the arguments
	// is built on the render thread inside ToolRegistry::invoke, so no reader state
	// ever crosses a thread boundary.
	//
	// Same DLL-boundary rule as ToolRegistry: get() is DEFINED IN THE .CPP only.
	class VE_API ToolDispatchQueue {
	public:
		static ToolDispatchQueue& get();

		// Queue a call and hand back a future for its result. Safe from any thread.
		std::future<std::string> submit(std::string name, std::string argsJson);

		// Run everything queued, on the calling (render) thread, and fulfil each
		// promise. Must be called every frame BEFORE any early-out that could skip
		// it — a submitter blocked on its future has no other way to make progress.
		void drain();

	private:
		struct Call {
			std::string name;
			std::string argsJson;
			std::promise<std::string> promise;
		};

		std::mutex m_mutex;
		std::deque<Call> m_queue;
	};

}
