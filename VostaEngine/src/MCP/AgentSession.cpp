#include "vepch.h"
#include "MCP/AgentSession.h"

#include "MCP/ToolDispatchQueue.h"
#include "MCP/ToolRegistry.h"

#include <chrono>
#include <thread>

namespace ve {

	namespace {

		// A turn may call tools N times before it must produce an answer. Bounded so a
		// model stuck in a call/observe loop cannot burn tokens forever.
		constexpr int kMaxToolIterations = 32;

		// Attempts per POST when retrying is on: the first try plus nine retries.
		constexpr int kMaxPostAttempts = 10;

		// Accept either a base URL ("https://host") or a full one
		// ("https://host/v1/chat/completions") — both end up at the endpoint.
		std::string completionsUrl(const std::string& endpoint) {
			if (endpoint.find("/chat/completions") != std::string::npos)
				return endpoint;

			std::string base = endpoint;
			while (!base.empty() && base.back() == '/')
				base.pop_back();
			return base + "/v1/chat/completions";
		}

	}

	AgentSession::AgentSession() = default;

	AgentSession::~AgentSession() {
		shutdown();
	}

	void AgentSession::configure(std::string endpoint, std::string apiKey, std::string model, bool retryOnError) {
		std::lock_guard lock(m_mutex);
		m_endpoint = std::move(endpoint);
		m_apiKey = std::move(apiKey);
		m_model = std::move(model);
		m_retryOnError = retryOnError;
	}

	bool AgentSession::configured() const {
		std::lock_guard lock(m_mutex);
		return !m_endpoint.empty() && !m_apiKey.empty() && !m_model.empty();
	}

	bool AgentSession::submit(std::string userText) {
		{
			std::lock_guard lock(m_mutex);
			if (m_busy || m_endpoint.empty() || m_apiKey.empty() || m_model.empty())
				return false;

			ChatMessage user;
			user.role = ChatMessage::Role::User;
			user.content = std::move(userText);
			m_history.push_back(std::move(user));

			m_busy = true;
			m_cancel = false;
			m_status = Status::Thinking;
			m_detail.clear();
			++m_revision;
		}

		m_handle = JobSystem::get().schedule([this] { runTurn(); });
		return true;
	}

	AgentSession::Snapshot AgentSession::snapshot() const {
		std::lock_guard lock(m_mutex);
		Snapshot out;
		out.status = m_status;
		out.busy = m_busy;
		out.streaming = m_streaming;
		out.detail = m_detail;
		out.streamText = m_streamText;
		out.revision = m_revision;
		out.messages = m_history;
		return out;
	}

	void AgentSession::clearHistory() {
		std::lock_guard lock(m_mutex);
		if (m_busy)
			return;
		m_history.clear();
		++m_revision;
	}

	void AgentSession::shutdown() {
		{
			std::lock_guard lock(m_mutex);
			m_cancel = true;
		}

		// Breaks a blocked WinHttpReceiveResponse; without it the worker could sit
		// in its 120 s receive timeout while we wait below.
		m_http.cancel();

		if (m_handle.future.valid()) {
			using namespace std::chrono_literals;
			while (m_handle.future.wait_for(0ms) != std::future_status::ready) {
				// The worker may itself be blocked on a tool future. Fulfilling it is
				// our job — this is the render thread.
				ToolDispatchQueue::get().drain();
				std::this_thread::sleep_for(1ms);
			}
			m_handle.future.wait();
		}
	}

	bool AgentSession::cancelled() const {
		std::lock_guard lock(m_mutex);
		return m_cancel;
	}

	void AgentSession::setStatus(Status status, std::string detail) {
		std::lock_guard lock(m_mutex);
		m_status = status;
		m_detail = std::move(detail);
		++m_revision;
	}

	void AgentSession::appendMessage(ChatMessage message) {
		std::lock_guard lock(m_mutex);
		m_history.push_back(std::move(message));
		++m_revision;
	}

	std::vector<LlmClient::ToolSchema> AgentSession::toolSchemas() const {
		std::vector<LlmClient::ToolSchema> out;
		for (const ToolInfo& t : ToolRegistry::get().list())
			out.push_back({ t.name, t.description, t.inputSchema });
		return out;
	}

	void AgentSession::runTurn() {
		bool producedAnswer = false;
		std::string error;

		for (int iteration = 0; iteration < kMaxToolIterations; ++iteration) {
			if (cancelled())
				break;

			std::string endpoint, apiKey, model;
			bool retryOnError = true;
			std::vector<ChatMessage> history;
			{
				std::lock_guard lock(m_mutex);
				endpoint = m_endpoint;
				apiKey = m_apiKey;
				model = m_model;
				retryOnError = m_retryOnError;
				history = m_history;            // copy: the lock is not held across the POST
			}

			const std::string body = LlmClient::buildRequest(model, history, toolSchemas());
			const std::vector<std::string> headers = {
				"Content-Type: application/json",
				"Authorization: Bearer " + apiKey,
			};

			// A transport error (e.g. WinHTTP 12007) or a 5xx/429 is worth another try:
			// these are transient. A 4xx is not — the request itself is wrong.
			const int maxAttempts = retryOnError ? kMaxPostAttempts : 1;
			HttpClient::Response response;
			LlmStreamAccumulator acc;
			for (int attempt = 0; attempt < maxAttempts; ++attempt) {
				if (cancelled())
					break;

				acc.reset();
				{
					std::lock_guard lock(m_mutex);
					m_streamText.clear();
					m_streaming = true;
					++m_revision;
				}

				response = m_http.post(completionsUrl(endpoint), headers, body,
					[this, &acc](const char* data, size_t len) {
						acc.feed(data, len);
						std::lock_guard lock(m_mutex);
						m_streamText = acc.content();
						++m_revision;
					});

				{
					std::lock_guard lock(m_mutex);
					m_streaming = false;
					++m_revision;
				}

				if (cancelled() || response.ok())
					break;

				// Once text has streamed, retrying would duplicate what is already on
				// screen — surface the failure instead.
				const bool retryable = acc.content().empty() && acc.toolCalls().empty() &&
									   (!response.error.empty() || response.status >= 500 ||
										response.status == 429);
				if (!retryable || attempt + 1 >= maxAttempts)
					break;

				const std::string why = !response.error.empty()
									  ? response.error
									  : "HTTP " + std::to_string(response.status);
				setStatus(Status::Thinking, "retrying (" + std::to_string(attempt + 2)
										   + "/" + std::to_string(maxAttempts) + ") after " + why);

				// Backoff, sliced so a cancel or shutdown interrupts it promptly.
				using namespace std::chrono_literals;
				const int delayMs = 400 * (attempt + 1);
				for (int slept = 0; slept < delayMs && !cancelled(); slept += 50)
					std::this_thread::sleep_for(50ms);
			}

			if (cancelled())
				break;

			if (!response.ok()) {
				error = !response.error.empty()
					  ? response.error
					  : LlmClient::extractError(response.body);
				if (error.empty())
					error = "request failed (HTTP " + std::to_string(response.status) + ")";
				break;
			}

			// Clear any "retrying ..." detail now that the request has landed.
			setStatus(Status::Thinking, std::string());

			if (!acc.error().empty()) {
				error = acc.error();
				break;
			}
			if (!acc.sawEvent()) {
				// The endpoint answered but not as an event stream — it ignored
				// stream:true. Fall back to whatever the body says.
				error = LlmClient::extractError(response.body);
				if (error.empty())
					error = "reply was not an event stream (HTTP " +
							std::to_string(response.status) + ")";
				break;
			}

			std::string content = acc.content();
			std::vector<ChatMessage::ToolCall> calls;
			for (const ChatMessage::ToolCall& c : acc.toolCalls()) {
				if (c.name.empty())
					continue;                   // a nameless fragment cannot be run
				ChatMessage::ToolCall tc = c;
				if (tc.id.empty())
					tc.id = "call_" + std::to_string(calls.size());
				if (tc.argumentsJson.empty())
					tc.argumentsJson = "{}";
				calls.push_back(std::move(tc));
			}

			if (calls.empty()) {
				// No tool calls: this text is the final answer.
				ChatMessage assistant;
				assistant.role = ChatMessage::Role::Assistant;
				assistant.content = std::move(content);
				appendMessage(std::move(assistant));
				producedAnswer = true;
				break;
			}

			ChatMessage assistant;
			assistant.role = ChatMessage::Role::Assistant;
			assistant.content = std::move(content);
			assistant.toolCalls = calls;        // recorded so the next request echoes them
			appendMessage(std::move(assistant));

			for (const ChatMessage::ToolCall& call : calls) {
				if (cancelled())
					break;

				setStatus(Status::RunningTool, call.name);
				const std::string toolResult =
					ToolDispatchQueue::get().submit(call.name, call.argumentsJson).get();

				ChatMessage tool;
				tool.role = ChatMessage::Role::Tool;
				tool.toolCallId = call.id;
				tool.content = toolResult;
				appendMessage(std::move(tool));
			}
		}

		if (error.empty() && !producedAnswer && !cancelled())
			error = "stopped after " + std::to_string(kMaxToolIterations) + " tool iterations";

		{
			std::lock_guard lock(m_mutex);
			m_busy = false;
			m_streaming = false;
			// The finished text is in m_history now, so the live copy retires.
			m_streamText.clear();
			if (m_cancel || producedAnswer) {
				m_status = Status::Idle;
				m_detail.clear();
			}
			else {
				m_status = Status::Error;
				m_detail = error;
			}
			++m_revision;
		}
	}

}
