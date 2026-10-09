#pragma once

#include "Core/Core.h"

#include <cstddef>
#include <string>
#include <vector>

namespace ve {

	// One entry in the conversation. Deliberately typed rather than opaque JSON:
	// only the wire layer below knows about tool_calls / tool_call_id, so callers
	// stay readable and never have to slice raw JSON back out of a reply.
	struct ChatMessage {
		enum class Role { User, Assistant, Tool };

		struct ToolCall {
			std::string id;
			std::string name;
			std::string argumentsJson;      // raw JSON string the model produced
		};

		Role role = Role::User;
		std::string content;                // user text / assistant text / tool result JSON
		std::vector<ToolCall> toolCalls;    // assistant only
		std::string toolCallId;             // tool only
	};

	// OpenAI-compatible /v1/chat/completions wire format. No transport, no threads:
	// this is only the request/response shape.
	class VE_API LlmClient {
	public:
		struct ToolSchema {
			std::string name;
			std::string description;
			std::string inputSchema;        // opaque JSON-Schema object string
		};

		// Build a streaming request body. `tools` may be empty.
		static std::string buildRequest(const std::string& model,
										const std::vector<ChatMessage>& history,
										const std::vector<ToolSchema>& tools);

		// Human-readable message out of an error body, or "" when there is none.
		static std::string extractError(const std::string& body);
	};

	// Accumulates an OpenAI-compatible streaming reply: SSE `data:` frames, each a
	// JSON chunk whose `choices[0].delta` carries content text and/or fragments of a
	// tool call. Feed it raw bytes as they arrive; read the result once the stream
	// ends. Every parse is total — a malformed frame is skipped, never fatal.
	class LlmStreamAccumulator {
	public:
		void feed(const char* data, size_t len);
		void reset();

		// True once a `data:` frame has been seen, so the caller can tell an event
		// stream from an endpoint that ignored `stream:true`.
		bool sawEvent() const { return m_sawEvent; }
		bool done() const { return m_done; }
		const std::string& content() const { return m_content; }
		const std::vector<ChatMessage::ToolCall>& toolCalls() const { return m_calls; }
		const std::string& error() const { return m_error; }

	private:
		void handleLine(const std::string& line);

		std::string m_pending;                      // tail of an incomplete line
		std::string m_content;
		std::vector<ChatMessage::ToolCall> m_calls; // indexed by tool_calls[].index
		std::string m_error;
		bool m_done = false;
		bool m_sawEvent = false;
	};

}
