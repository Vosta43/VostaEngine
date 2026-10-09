#include "vepch.h"
#include "MCP/ToolDispatchQueue.h"

#include "MCP/ToolRegistry.h"

#include <utility>

namespace ve {

	ToolDispatchQueue& ToolDispatchQueue::get() {
		static ToolDispatchQueue instance;
		return instance;
	}

	std::future<std::string> ToolDispatchQueue::submit(std::string name, std::string argsJson) {
		Call call;
		call.name = std::move(name);
		call.argsJson = std::move(argsJson);

		// Take the future before the promise moves into the queue.
		std::future<std::string> future = call.promise.get_future();

		{
			std::lock_guard<std::mutex> lock(m_mutex);
			m_queue.push_back(std::move(call));
		}
		return future;
	}

	void ToolDispatchQueue::drain() {
		for (;;) {
			Call call;
			{
				std::lock_guard<std::mutex> lock(m_mutex);
				if (m_queue.empty())
					break;
				call = std::move(m_queue.front());
				m_queue.pop_front();
			}

			// Compute outside the set_value call, and always fulfil. If invoke
			// escaped with an exception and the promise were left unfulfilled, the
			// submitter would block forever.
			std::string result;
			try {
				result = ToolRegistry::get().invoke(call.name, call.argsJson);
			}
			catch (const std::exception& e) {
				result = std::string("{\"error\":\"") + call.name + " failed: " + e.what() + "\"}";
			}
			catch (...) {
				result = "{\"error\":\"" + call.name + " failed: unknown exception\"}";
			}

			call.promise.set_value(std::move(result));
		}
	}

}
