#pragma once

#include "MCP/HttpClient.h"
#include "MCP/LlmClient.h"
#include "Core/Core.h"
#include "Core/JobSystem.h"

#include <cstdint>
#include <mutex>
#include <string>
#include <vector>

namespace ve {

	// One conversation with an OpenAI-compatible model, plus the agent loop that
	// lets it call the engine's MCP tools. The turn runs on a JobSystem worker so
	// the render thread keeps drawing; the UI reads state through snapshot(), never
	// by touching the history directly.
	//
	// Tool handlers run on the render thread (they touch the Scene and GL), so a
	// tool call here is a ToolDispatchQueue::submit() whose future the render
	// thread fulfils from its per-frame drain(). The worker simply blocks on it —
	// which is why shutdown() must keep draining until the turn finishes.
	class VE_API AgentSession {
	public:
		enum class Status { Idle, Thinking, RunningTool, Error };

		struct Snapshot {
			Status status = Status::Idle;
			bool busy = false;
			bool streaming = false;             // a reply is arriving chunk by chunk
			std::string detail;                 // tool name while running, else error text
			std::string streamText;             // reply text so far while streaming
			uint64_t revision = 0;              // bumped on any history/status change
			std::vector<ChatMessage> messages;  // full copy, taken under the lock
		};

		AgentSession();
		~AgentSession();

		AgentSession(const AgentSession&) = delete;
		AgentSession& operator=(const AgentSession&) = delete;

		void configure(std::string endpoint, std::string apiKey, std::string model, bool retryOnError);
		bool configured() const;

		// Append the user turn and start a background turn. False when already busy
		// or unconfigured — the caller disables Send on that.
		bool submit(std::string userText);

		Snapshot snapshot() const;

		// Dropped when a turn is in flight, so the worker never reads a half history.
		void clearHistory();

		// Cancel the in-flight request and block until the worker has stopped. Safe
		// to call twice, and from the layer teardown path.
		void shutdown();

	private:
		void runTurn();
		void setStatus(Status status, std::string detail);
		void appendMessage(ChatMessage message);
		bool cancelled() const;
		std::vector<LlmClient::ToolSchema> toolSchemas() const;

		mutable std::mutex m_mutex;
		std::vector<ChatMessage> m_history;
		Status m_status = Status::Idle;
		std::string m_detail;
		std::string m_streamText;           // partial reply, published for the UI
		bool m_streaming = false;
		uint64_t m_revision = 0;
		bool m_busy = false;
		bool m_cancel = false;

		std::string m_endpoint, m_apiKey, m_model;
		// Retry a failed request (transport error, 5xx, 429) before giving up.
		bool m_retryOnError = true;

		HttpClient m_http;
		// Written by submit(), read by shutdown(), both on the render thread.
		TaskHandle<void> m_handle;
	};

}
