#include "AiPanel.h"

#include "Core/AssetConfig.h"
#include "Core/Json.h"

#include <imgui.h>

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <unordered_map>

namespace ve {

    namespace {

        const char* kSettingsFile = "llm_settings.json";
        const char* kApiKeyEnv = "VOSTA_LLM_API_KEY";

        // getenv trips the CRT's deprecation error; _dupenv_s avoids it and hands
        // back a copy rather than a pointer into a shared static buffer.
        std::string readEnv(const char* name) {
            char* value = nullptr;
            size_t length = 0;
            if (_dupenv_s(&value, &length, name) != 0 || !value)
                return std::string();
            std::string out(value);
            std::free(value);
            return out;
        }

        void copyToBuf(char* dst, size_t cap, const std::string& src) {
            if (cap == 0)
                return;
            const size_t n = (src.size() < cap - 1) ? src.size() : cap - 1;
            std::memcpy(dst, src.data(), n);
            dst[n] = '\0';
        }

        // Flatten the transcript to plain text for the clipboard: role label, the
        // message body, then any tool calls it made on their own indented lines.
        std::string buildTranscript(const std::vector<ChatMessage>& messages) {
            std::string out;
            for (const ChatMessage& m : messages) {
                switch (m.role) {
                case ChatMessage::Role::User:      out += "You: "; break;
                case ChatMessage::Role::Assistant: out += "Assistant: "; break;
                case ChatMessage::Role::Tool:      out += "Tool result: "; break;
                }
                out += m.content;
                out += '\n';
                for (const ChatMessage::ToolCall& call : m.toolCalls) {
                    out += "  calls ";
                    out += call.name;
                    out += ' ';
                    out += call.argumentsJson;
                    out += '\n';
                }
                out += '\n';
            }
            return out;
        }

        const ImVec4 kUserColor{ 0.55f, 0.75f, 1.0f, 1.0f };
        const ImVec4 kAssistantColor{ 0.68f, 0.68f, 0.68f, 1.0f };
        const ImVec4 kErrorColor{ 1.0f, 0.45f, 0.45f, 1.0f };

        // Flatten a JSON argument blob to one short line, for a collapsed step title.
        std::string oneLine(const std::string& s, size_t maxLen) {
            std::string out;
            bool inSpace = false;
            for (char c : s) {
                const bool space = (c == ' ' || c == '\t' || c == '\r' || c == '\n');
                if (space) {
                    inSpace = true;
                    continue;
                }
                if (inSpace && !out.empty())
                    out += ' ';
                inSpace = false;
                out += c;
                if (out.size() >= maxLen) {
                    out += "...";
                    break;
                }
            }
            return out;
        }

        ImVec4 scaleColor(const ImVec4& c, float f) {
            return ImVec4(std::min(c.x * f, 1.0f), std::min(c.y * f, 1.0f),
                          std::min(c.z * f, 1.0f), c.w);
        }

        // What a tool does, so steps of the same nature share a colour.
        enum class ToolKind { Read, Scene, Asset, Editor, Other };

        ToolKind toolKind(const std::string& name) {
            if (name == "editor_action")
                return ToolKind::Editor;
            if (name.rfind("list_", 0) == 0 || name.rfind("get_", 0) == 0 ||
                name.rfind("read_", 0) == 0 || name.rfind("scene_", 0) == 0)
                return ToolKind::Read;
            if (name.rfind("create_asset", 0) == 0 || name.rfind("write_asset", 0) == 0)
                return ToolKind::Asset;
            if (name.rfind("create_", 0) == 0 || name.rfind("spawn_", 0) == 0 ||
                name.rfind("delete_", 0) == 0 || name.rfind("set_", 0) == 0)
                return ToolKind::Scene;
            return ToolKind::Other;
        }

        ImVec4 toolColor(const std::string& name) {
            switch (toolKind(name)) {
            case ToolKind::Read:   return ImVec4(0.30f, 0.38f, 0.50f, 1.0f);   // slate
            case ToolKind::Scene:  return ImVec4(0.26f, 0.46f, 0.34f, 1.0f);   // green
            case ToolKind::Asset:  return ImVec4(0.52f, 0.40f, 0.22f, 1.0f);   // amber
            case ToolKind::Editor: return ImVec4(0.42f, 0.30f, 0.50f, 1.0f);   // violet
            default:               return ImVec4(0.34f, 0.34f, 0.34f, 1.0f);   // grey
            }
        }

        // ImGui's only mouse-selectable text widget is a read-only input. Sizing it to
        // its wrapped content and hiding the frame makes it read as plain text.
        void selectableText(const char* id, const std::string& text) {
            std::string s = text;
            while (!s.empty() && (s.back() == '\n' || s.back() == '\r'))
                s.pop_back();
            if (s.empty())
                return;

            const ImGuiStyle& style = ImGui::GetStyle();
            float wrapW = ImGui::GetContentRegionAvail().x - style.FramePadding.x * 2.0f;
            if (wrapW < 1.0f)
                wrapW = 1.0f;
            const float textH = ImGui::CalcTextSize(s.c_str(), nullptr, false, wrapW).y;

            ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
            ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, ImVec4(1.0f, 1.0f, 1.0f, 0.06f));
            ImGui::PushStyleColor(ImGuiCol_FrameBgActive, ImVec4(1.0f, 1.0f, 1.0f, 0.10f));
            ImGui::InputTextMultiline(id, s.data(), s.size() + 1,
                                      ImVec2(-1.0f, textH + style.FramePadding.y * 2.0f + 1.0f),
                                      ImGuiInputTextFlags_ReadOnly);
            ImGui::PopStyleColor(3);
        }

        std::filesystem::path settingsPath() {
            return getAssetRoot() / "Saved" / kSettingsFile;
        }

    }

    void AiPanel::loadSettings() {
        copyToBuf(m_endpointBuf, sizeof(m_endpointBuf), "https://api.openai.com");
        copyToBuf(m_modelBuf, sizeof(m_modelBuf), "gpt-4o-mini");

        JsonReader r;
        if (!JsonReader::load(settingsPath().string(), r) || !r.valid())
            return;

        const std::string endpoint = r.getString("endpoint", "");
        const std::string model = r.getString("model", "");
        if (!endpoint.empty())
            copyToBuf(m_endpointBuf, sizeof(m_endpointBuf), endpoint);
        if (!model.empty())
            copyToBuf(m_modelBuf, sizeof(m_modelBuf), model);
    }

    void AiPanel::saveSettings() const {
        std::error_code ec;
        std::filesystem::create_directories(settingsPath().parent_path(), ec);

        JsonWriter w;
        w.set("endpoint", std::string(m_endpointBuf));
        w.set("model", std::string(m_modelBuf));
        // The API key is deliberately absent: this file is meant to be shareable.
        w.writeToFile(settingsPath().string());
    }

    std::string AiPanel::apiKey() const {
        const std::string env = readEnv(kApiKeyEnv);
        return env.empty() ? std::string(m_apiKeyBuf) : env;
    }

    void AiPanel::shutdown() {
        m_session.shutdown();
    }

    void AiPanel::onGuiRender(bool* openFlag) {
        if (!m_loaded) {
            loadSettings();
            m_loaded = true;
        }

        ImGui::SetNextWindowSize(ImVec2(560.0f, 720.0f), ImGuiCond_FirstUseEver);
        if (!ImGui::Begin("AI Assistant", openFlag)) {
            ImGui::End();
            return;
        }

        const AgentSession::Snapshot snap = m_session.snapshot();

        // Esc interrupts a turn, but only while this panel has focus.
        if (snap.busy && ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) &&
            ImGui::IsKeyPressed(ImGuiKey_Escape))
            m_session.shutdown();

        drawSettings();
        ImGui::Separator();
        drawChat(snap);
        ImGui::Separator();
        drawToolConsole();

        ImGui::End();
    }

    void AiPanel::drawSettings() {
        if (!ImGui::CollapsingHeader("Settings"))
            return;

        ImGui::SetNextItemWidth(-1.0f);
        ImGui::InputText("##endpoint", m_endpointBuf, sizeof(m_endpointBuf));
        ImGui::TextDisabled("Endpoint - base URL, or the full .../v1/chat/completions");

        ImGui::SetNextItemWidth(-1.0f);
        ImGui::InputText("##model", m_modelBuf, sizeof(m_modelBuf));

        ImGui::SetNextItemWidth(-1.0f);
        ImGui::InputText("##apiKey", m_apiKeyBuf, sizeof(m_apiKeyBuf), ImGuiInputTextFlags_Password);

        const bool fromEnv = !readEnv(kApiKeyEnv).empty();
        if (fromEnv)
            ImGui::TextDisabled("%s is set - it takes priority over this field.", kApiKeyEnv);
        else
            ImGui::TextDisabled("API key - held in memory only, never written to disk.");

        ImGui::Checkbox("Retry on error", &m_retryOnError);
        ImGui::SameLine();
        ImGui::TextDisabled("retry a failed request before giving up");

        if (ImGui::Button("Save settings"))
            saveSettings();
        ImGui::SameLine();
        ImGui::TextDisabled("(endpoint + model)");
    }

    void AiPanel::submitInput() {
        m_session.configure(m_endpointBuf, apiKey(), m_modelBuf, m_retryOnError);
        if (m_session.submit(m_inputBuf)) {
            m_inputBuf[0] = '\0';
            m_follow = true;                // a fresh question jumps to the newest step
        }
    }

    void AiPanel::drawChat(const AgentSession::Snapshot& snap) {
        // Flattened text for the Copy button; the on-screen chat is built from the
        // messages below, as collapsible per-action units.
        m_transcript = buildTranscript(snap.messages);

        ImGui::BeginChild("##chat", ImVec2(-1.0f, -110.0f), ImGuiChildFlags_Borders);

        // Wheel intent, not scroll position, decides whether new content drags the
        // view down: wheeling up means the user is reading back, so stop following.
        // ChildWindows so the wheel over a selectable text block counts too.
        if (ImGui::IsWindowHovered(ImGuiHoveredFlags_ChildWindows) &&
            ImGui::GetIO().MouseWheel != 0.0f)
            m_follow = ImGui::GetIO().MouseWheel < 0.0f;

        drawConversation(snap);

        // The reply as it arrives, before it lands in the history as a finished
        // message. Drawn last so it sits above the status line.
        if (snap.streaming && !snap.streamText.empty()) {
            ImGui::PushID(-1);
            ImGui::PushStyleColor(ImGuiCol_Text, kAssistantColor);
            ImGui::TextUnformatted("Assistant");
            ImGui::PopStyleColor();
            selectableText("##live", snap.streamText);
            ImGui::PopID();
            ImGui::Spacing();
        }

        if (snap.busy) {
            if (snap.status == AgentSession::Status::RunningTool)
                ImGui::TextDisabled("Running %s ...", snap.detail.c_str());
            else if (!snap.detail.empty())
                ImGui::TextDisabled("%s ...", snap.detail.c_str());
            else if (!snap.streaming || snap.streamText.empty())
                ImGui::TextDisabled("Waiting for the model ...");
        }
        else if (snap.status == AgentSession::Status::Error) {
            ImGui::PushStyleColor(ImGuiCol_Text, kErrorColor);
            ImGui::PushTextWrapPos(0.0f);
            ImGui::TextWrapped("Error: %s", snap.detail.c_str());
            ImGui::PopTextWrapPos();
            ImGui::PopStyleColor();
        }

        // GetScrollMaxY is one frame stale, so pinning every frame lands exactly on
        // the bottom the frame after new content arrives.
        if (m_follow)
            ImGui::SetScrollY(ImGui::GetScrollMaxY());

        ImGui::EndChild();

        ImGui::InputTextMultiline("##input", m_inputBuf, sizeof(m_inputBuf), ImVec2(-1.0f, 60.0f));

        const bool canSend = !snap.busy && m_inputBuf[0] != '\0' &&
                             m_endpointBuf[0] != '\0' && m_modelBuf[0] != '\0' &&
                             !apiKey().empty();

        // Ctrl+Enter sends. The multiline box has already inserted a newline this
        // frame, so drop the trailing one before submitting.
        if (ImGui::IsItemFocused() && ImGui::GetIO().KeyCtrl &&
            ImGui::IsKeyPressed(ImGuiKey_Enter)) {
            size_t n = std::strlen(m_inputBuf);
            while (n > 0 && (m_inputBuf[n - 1] == '\n' || m_inputBuf[n - 1] == '\r'))
                m_inputBuf[--n] = '\0';
            if (canSend)
                submitInput();
        }

        ImGui::BeginDisabled(!canSend);
        if (ImGui::Button("Send", ImVec2(90.0f, 0.0f)))
            submitInput();
        ImGui::EndDisabled();

        ImGui::SameLine();
        ImGui::BeginDisabled(!snap.busy);
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.55f, 0.18f, 0.18f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.70f, 0.24f, 0.24f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.42f, 0.14f, 0.14f, 1.0f));
        if (ImGui::Button("Interrupt"))
            m_session.shutdown();       // cancels the in-flight turn; the session stays usable
        ImGui::PopStyleColor(3);
        ImGui::EndDisabled();

        ImGui::SameLine();
        ImGui::BeginDisabled(snap.busy);
        if (ImGui::Button("Clear"))
            m_session.clearHistory();
        ImGui::EndDisabled();

        ImGui::SameLine();
        ImGui::BeginDisabled(m_transcript.empty());
        if (ImGui::Button("Copy"))
            ImGui::SetClipboardText(m_transcript.c_str());
        ImGui::EndDisabled();
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Copy the whole conversation to the clipboard");

        if (!snap.busy && m_endpointBuf[0] != '\0' && apiKey().empty())
            ImGui::TextDisabled("Set an API key (or VOSTA_LLM_API_KEY) to chat.");
    }

    void AiPanel::drawConversation(const AgentSession::Snapshot& snap) {
        // Tool results arrive as separate Tool messages; key them by call id so each
        // action unit can show its own result.
        std::unordered_map<std::string, std::string> results;
        for (const ChatMessage& m : snap.messages)
            if (m.role == ChatMessage::Role::Tool)
                results[m.toolCallId] = m.content;

        int blockId = 0;
        for (const ChatMessage& m : snap.messages) {
            switch (m.role) {
            case ChatMessage::Role::User:
                ImGui::PushID(blockId++);
                ImGui::PushStyleColor(ImGuiCol_Text, kUserColor);
                ImGui::TextUnformatted("You");
                ImGui::PopStyleColor();
                selectableText("##text", m.content);
                ImGui::PopID();
                ImGui::Spacing();
                break;

            case ChatMessage::Role::Assistant:
                if (!m.content.empty()) {
                    ImGui::PushID(blockId++);
                    ImGui::PushStyleColor(ImGuiCol_Text, kAssistantColor);
                    ImGui::TextUnformatted("Assistant");
                    ImGui::PopStyleColor();
                    selectableText("##text", m.content);
                    ImGui::PopID();
                    ImGui::Spacing();
                }
                for (const ChatMessage::ToolCall& call : m.toolCalls)
                    drawToolCall(call, results, snap, blockId++);
                break;

            case ChatMessage::Role::Tool:
                break;                      // shown inside its call's unit
            }
        }
    }

    void AiPanel::drawToolCall(const ChatMessage::ToolCall& call,
                               const std::unordered_map<std::string, std::string>& results,
                               const AgentSession::Snapshot& snap, int id) {
        const auto it = results.find(call.id);
        const bool hasResult = it != results.end();
        const bool running = !hasResult && snap.busy &&
                             snap.status == AgentSession::Status::RunningTool &&
                             snap.detail == call.name;

        std::string title = call.name.empty() ? "tool" : call.name;
        const std::string args = oneLine(call.argumentsJson, 56);
        if (!args.empty() && args != "{}")
            title += "  " + args;
        if (running)
            title += "  ...";

        // A stable id per step keeps the open/closed state across frames; the label
        // alone would collide when the same tool runs twice.
        const ImVec4 base = toolColor(call.name);
        ImGui::PushID(id);
        ImGui::PushStyleColor(ImGuiCol_Header, base);
        ImGui::PushStyleColor(ImGuiCol_HeaderHovered, scaleColor(base, 1.35f));
        ImGui::PushStyleColor(ImGuiCol_HeaderActive, scaleColor(base, 0.75f));
        ImGui::PushStyleColor(ImGuiCol_Text, scaleColor(base, 1.9f));
        const bool open = ImGui::CollapsingHeader(title.c_str());
        ImGui::PopStyleColor(4);

        if (open) {
            ImGui::TextDisabled("arguments");
            selectableText("##args", call.argumentsJson);
            ImGui::Spacing();
            ImGui::TextDisabled("result");
            if (hasResult)
                selectableText("##result", it->second);
            else
                ImGui::TextDisabled(running ? "running ..." : "(no result)");
            ImGui::Spacing();
        }
        ImGui::PopID();
    }

    void AiPanel::drawToolConsole() {
        if (!ImGui::CollapsingHeader("Tool Console", ImGuiTreeNodeFlags_DefaultOpen))
            return;

        const std::vector<ToolInfo>& tools = ToolRegistry::get().list();
        if (tools.empty()) {
            ImGui::TextDisabled("No tools registered.");
            return;
        }

        const bool hasSelection = m_consoleTool >= 0 && m_consoleTool < static_cast<int>(tools.size());
        const char* preview = hasSelection ? tools[m_consoleTool].name.c_str() : "<select a tool>";

        if (ImGui::BeginCombo("##toolCombo", preview)) {
            for (int i = 0; i < static_cast<int>(tools.size()); ++i) {
                const bool selected = (i == m_consoleTool);
                if (ImGui::Selectable(tools[i].name.c_str(), selected))
                    m_consoleTool = i;
                if (selected)
                    ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }

        if (hasSelection) {
            const ToolInfo& tool = tools[m_consoleTool];
            ImGui::PushTextWrapPos(0.0f);
            ImGui::TextDisabled("%s", tool.description.c_str());
            ImGui::TextDisabled("inputSchema:");
            ImGui::TextUnformatted(tool.inputSchema.c_str());
            ImGui::PopTextWrapPos();

            ImGui::InputTextMultiline("##toolArgs", m_argsBuf, sizeof(m_argsBuf), ImVec2(-1.0f, 70.0f));
            if (ImGui::Button("Run"))
                m_consoleResult = ToolRegistry::get().invoke(tool.name, m_argsBuf);

            if (!m_consoleResult.empty()) {
                ImGui::BeginChild("##toolResult", ImVec2(0.0f, 140.0f), ImGuiChildFlags_Borders);
                ImGui::PushTextWrapPos(0.0f);
                ImGui::TextUnformatted(m_consoleResult.c_str());
                ImGui::PopTextWrapPos();
                ImGui::EndChild();
            }
        }
    }

}
