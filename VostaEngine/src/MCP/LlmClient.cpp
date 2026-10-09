#include "vepch.h"
#include "MCP/LlmClient.h"

#include "Core/Json.h"

namespace ve {

	std::string LlmClient::buildRequest(const std::string& model,
										const std::vector<ChatMessage>& history,
										const std::vector<ToolSchema>& tools) {
		JsonWriter w;
		w.set("model", model);
		w.set("stream", true);

		w.beginArray("messages", history.size());
		for (const ChatMessage& m : history) {
			w.beginObject();
			switch (m.role) {
			case ChatMessage::Role::Tool:
				w.set("role", "tool");
				w.set("tool_call_id", m.toolCallId);
				w.set("content", m.content);
				break;

			case ChatMessage::Role::Assistant:
				w.set("role", "assistant");
				w.set("content", m.content);
				if (!m.toolCalls.empty()) {
					w.beginArray("tool_calls", m.toolCalls.size());
					for (const ChatMessage::ToolCall& tc : m.toolCalls) {
						w.beginObject();
						w.set("id", tc.id);
						w.set("type", "function");
						w.beginObject("function");
						w.set("name", tc.name);
						// arguments is the model's raw JSON *string* — escaped and
						// re-sent verbatim, never re-encoded as an object.
						w.set("arguments", tc.argumentsJson);
						w.end();
						w.end();
					}
					w.end();
				}
				break;

			case ChatMessage::Role::User:
			default:
				w.set("role", "user");
				w.set("content", m.content);
				break;
			}
			w.end();
		}
		w.end();

		if (!tools.empty()) {
			w.beginArray("tools", tools.size());
			for (const ToolSchema& t : tools) {
				w.beginObject();
				w.set("type", "function");
				w.beginObject("function");
				w.set("name", t.name);
				w.set("description", t.description);
				w.setRaw("parameters", t.inputSchema);
				w.end();
				w.end();
			}
			w.end();
		}

		return w.str();
	}

	void LlmStreamAccumulator::reset() {
		m_pending.clear();
		m_content.clear();
		m_calls.clear();
		m_error.clear();
		m_done = false;
		m_sawEvent = false;
	}

	void LlmStreamAccumulator::feed(const char* data, size_t len) {
		if (!data || len == 0)
			return;
		m_pending.append(data, len);

		// SSE is line-delimited; hold the tail back for the next block.
		size_t start = 0;
		for (;;) {
			const size_t nl = m_pending.find('\n', start);
			if (nl == std::string::npos)
				break;
			std::string line = m_pending.substr(start, nl - start);
			if (!line.empty() && line.back() == '\r')
				line.pop_back();
			handleLine(line);
			start = nl + 1;
		}
		if (start > 0)
			m_pending.erase(0, start);
	}

	void LlmStreamAccumulator::handleLine(const std::string& line) {
		// Blank lines separate events, ": ..." is a comment, and anything that is not
		// "data:" is an event name or id we have no use for.
		if (line.empty() || line[0] == ':')
			return;
		if (line.rfind("data:", 0) != 0)
			return;

		std::string payload = line.substr(5);
		while (!payload.empty() && payload.front() == ' ')
			payload.erase(payload.begin());
		if (payload.empty())
			return;
		if (payload == "[DONE]") {
			m_done = true;
			return;
		}

		JsonReader r;
		if (!JsonReader::parse(payload, r) || !r.valid())
			return;                       // skip a malformed frame, never fail the turn

		m_sawEvent = true;

		if (r.has("error")) {
			const std::string msg = LlmClient::extractError(payload);
			if (!msg.empty() && m_error.empty())
				m_error = msg;
			return;
		}

		// Some providers put usage in a trailing chunk with no choices at all.
		const JsonReader choices = r.child("choices");
		if (choices.size() == 0)
			return;
		const JsonReader delta = choices.atIndex(0).child("delta");

		const std::string text = delta.getString("content", "");
		if (!text.empty())
			m_content += text;

		// Tool calls stream in fragments: id arrives once, name and arguments piece by
		// piece, all keyed by `index`.
		const JsonReader calls = delta.child("tool_calls");
		for (size_t i = 0; i < calls.size(); ++i) {
			const JsonReader c = calls.atIndex(i);
			const int index = c.getInt("index", static_cast<int>(i));
			if (index < 0)
				continue;
			if (m_calls.size() <= static_cast<size_t>(index))
				m_calls.resize(static_cast<size_t>(index) + 1);

			ChatMessage::ToolCall& tc = m_calls[static_cast<size_t>(index)];
			const std::string id = c.getString("id", "");
			if (!id.empty())
				tc.id = id;
			const JsonReader fn = c.child("function");
			tc.name += fn.getString("name", "");
			tc.argumentsJson += fn.getString("arguments", "");
		}
	}

	std::string LlmClient::extractError(const std::string& body) {
		JsonReader r;
		if (!JsonReader::parse(body, r) || !r.valid() || !r.has("error"))
			return std::string();

		const JsonReader err = r.child("error");
		// {"error":{"message":"..."}}
		const std::string nested = err.getString("message", "");
		if (!nested.empty())
			return nested;
		// {"error":"..."}
		const std::string flat = err.asString("");
		if (!flat.empty())
			return flat;
		return "provider returned an error";
	}

}
