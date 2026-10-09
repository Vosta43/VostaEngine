#pragma once

#include <VostaEngine.h>

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace ve {

    // One window with two halves:
    //   Chat        - talks to an OpenAI-compatible model; the model may call the
    //                 engine's MCP tools, whose results are fed back until it
    //                 answers. All of that runs inside AgentSession. Each action the
    //                 model takes shows as a collapsible unit, coloured by tool kind.
    //   Tool Console - runs a single tool by hand, with no model involved, to
    //                 test the tool layer on its own.
    //
    // Endpoint and model persist to Saved/llm_settings.json. The API key never
    // does: it comes from VOSTA_LLM_API_KEY or the in-memory field below.
    class AiPanel {
    public:
        void onGuiRender(bool* openFlag = nullptr);

        // Stops an in-flight turn. Called from the layer teardown path.
        void shutdown();

    private:
        void loadSettings();
        void saveSettings() const;
        std::string apiKey() const;

        // Configure + submit whatever is in the input box. Shared by the Send button
        // and the Ctrl+Enter shortcut.
        void submitInput();

        void drawSettings();
        void drawChat(const AgentSession::Snapshot& snap);
        void drawConversation(const AgentSession::Snapshot& snap);
        void drawToolCall(const ChatMessage::ToolCall& call,
                          const std::unordered_map<std::string, std::string>& results,
                          const AgentSession::Snapshot& snap, int id);
        void drawToolConsole();

        AgentSession m_session;

        char m_endpointBuf[512] = "";
        char m_modelBuf[128] = "";
        char m_apiKeyBuf[512] = "";         // memory only, never written to disk
        char m_inputBuf[4096] = "";
        char m_argsBuf[4096] = "{}";

        std::string m_transcript;           // flattened chat, for the Copy button
        // Cleared while the user wheels up to read back; new content then leaves
        // the view alone until they wheel back down.
        bool m_follow = true;
        // Retry a failed request (e.g. WinHTTP 12007) before surfacing the error.
        bool m_retryOnError = true;

        std::string m_consoleResult;
        int m_consoleTool = -1;

        bool m_loaded = false;
    };

}
