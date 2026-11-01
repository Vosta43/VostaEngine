#include "JsonEditorPanel.h"

#include "imgui.h"

#include <algorithm>
#include <cerrno>
#include <cmath>
#include <cstdlib>
#include <utility>
#include <vector>

namespace ve {

    namespace {

        const ImVec4 kKeyColor(0.80f, 0.80f, 0.82f, 1.0f);
        const ImVec4 kIndexColor(0.52f, 0.52f, 0.55f, 1.0f);

        ImVec4 kindColor(JsonKind kind) {
            switch (kind) {
            case JsonKind::Null:    return ImVec4(0.55f, 0.55f, 0.57f, 1.0f);
            case JsonKind::Bool:    return ImVec4(0.51f, 0.71f, 0.92f, 1.0f);
            case JsonKind::Integer: return ImVec4(1.00f, 0.69f, 0.27f, 1.0f);
            case JsonKind::Float:   return ImVec4(0.96f, 0.80f, 0.47f, 1.0f);
            case JsonKind::String:  return ImVec4(0.59f, 0.82f, 0.47f, 1.0f);
            case JsonKind::Object:  return ImVec4(0.67f, 0.49f, 0.94f, 1.0f);
            case JsonKind::Array:   return ImVec4(0.35f, 0.78f, 0.67f, 1.0f);
            }
            return kKeyColor;
        }

        const char* kindBadge(JsonKind kind) {
            switch (kind) {
            case JsonKind::Null:    return "null";
            case JsonKind::Bool:    return "bool";
            case JsonKind::Integer: return "int";
            case JsonKind::Float:   return "float";
            case JsonKind::String:  return "str";
            case JsonKind::Object:  return "{ }";
            case JsonKind::Array:   return "[ ]";
            }
            return "";
        }

        const char* kindName(JsonKind kind) {
            switch (kind) {
            case JsonKind::Null:    return "Null";
            case JsonKind::Bool:    return "Boolean";
            case JsonKind::Integer: return "Integer";
            case JsonKind::Float:   return "Float";
            case JsonKind::String:  return "String";
            case JsonKind::Object:  return "Object";
            case JsonKind::Array:   return "Array";
            }
            return "";
        }

        // std::string-backed InputText buffer: ImGui needs the buffer pointer to
        // follow a reallocation while the field grows past its capacity.
        int textResizeCallback(ImGuiInputTextCallbackData* data) {
            if (data->EventFlag == ImGuiInputTextFlags_CallbackResize) {
                auto* str = static_cast<std::string*>(data->UserData);
                str->resize(static_cast<size_t>(data->BufTextLen));
                data->Buf = str->data();
            }
            return 0;
        }

        // Split off the last path component. Avoids std::filesystem's narrow
        // conversion, which goes through the ANSI code page and can throw on a
        // UTF-8 path.
        std::string fileNameOf(const std::string& path) {
            const size_t sep = path.find_last_of("/\\");
            return sep == std::string::npos ? path : path.substr(sep + 1);
        }

        std::string uniqueKey(const JsonNode& object) {
            const std::string base = "new_key";
            std::string key = base;
            for (int n = 2;; ++n) {
                bool taken = false;
                for (size_t i = 0; i < object.childCount(); ++i) {
                    if (object.keyAt(i) == key) {
                        taken = true;
                        break;
                    }
                }
                if (!taken)
                    return key;
                key = base + "_" + std::to_string(n);
            }
        }

    }

    void JsonEditorPanel::openFile(const std::string& path) {
        if (m_open && path == m_path)
            return;

        if (m_open && hasUnsavedWork()) {
            m_pendingOpenPath = path;
            requestConfirm(ConfirmAction::OpenOther);
            return;
        }

        loadFile(path);
    }

    bool JsonEditorPanel::hasUnsavedWork() const {
        return m_doc.dirty() || !m_editPath.empty();
    }

    void JsonEditorPanel::loadFile(const std::string& path) {
        m_path = path;
        m_open = true;
        m_parseFailed = false;
        m_error.clear();
        m_status.clear();
        m_statusError = false;
        m_childLimit.clear();
        m_pending = PendingOp();
        endEdit();

        std::string error;
        if (!m_doc.load(path, &error)) {
            m_parseFailed = true;
            m_error = error.empty() ? std::string("Could not load this file.") : error;
        }
    }

    void JsonEditorPanel::saveFromUi() {
        if (!m_editPath.empty())
            commitEdit();
        if (m_editPath.empty())
            saveNow();
    }

    void JsonEditorPanel::saveNow() {
        if (!m_doc.valid()) {
            setStatus("Nothing to save.", true);
            return;
        }

        std::string error;
        if (!m_doc.save(&error))
            setStatus("Save failed: " + error, true);
        else
            setStatus("Saved.", false);
    }

    void JsonEditorPanel::requestConfirm(ConfirmAction action) {
        m_confirm = action;
        m_confirmRequested = true;
    }

    void JsonEditorPanel::applyAction(ConfirmAction action) {
        m_confirm = ConfirmAction::None;

        switch (action) {
        case ConfirmAction::Close:
            endEdit();
            m_open = false;
            break;
        case ConfirmAction::Revert:
            loadFile(m_path);
            break;
        case ConfirmAction::OpenOther: {
            const std::string path = m_pendingOpenPath;
            m_pendingOpenPath.clear();
            loadFile(path);
            break;
        }
        default:
            break;
        }
    }

    void JsonEditorPanel::drawConfirmPopup() {
        if (m_confirmRequested) {
            ImGui::OpenPopup("Unsaved changes###jsonConfirm");
            m_confirmRequested = false;
        }

        if (!ImGui::BeginPopupModal("Unsaved changes###jsonConfirm", nullptr,
                                    ImGuiWindowFlags_AlwaysAutoResize))
            return;

        const std::string name = m_path.empty()
            ? std::string("This file")
            : fileNameOf(m_path);
        ImGui::TextWrapped("%s has unsaved changes.", name.c_str());
        ImGui::Separator();

        if (ImGui::Button("Save", ImVec2(110.0f, 0.0f))) {
            saveFromUi();
            if (m_editPath.empty() && !m_doc.dirty()) {
                applyAction(m_confirm);
                ImGui::CloseCurrentPopup();
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("Discard", ImVec2(110.0f, 0.0f))) {
            applyAction(m_confirm);
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(110.0f, 0.0f))) {
            m_confirm = ConfirmAction::None;
            m_pendingOpenPath.clear();
            ImGui::CloseCurrentPopup();
        }

        if (m_statusError && !m_status.empty()) {
            ImGui::Separator();
            ImGui::TextColored(ImVec4(0.95f, 0.35f, 0.35f, 1.0f), "%s", m_status.c_str());
        }

        ImGui::EndPopup();
    }

    void JsonEditorPanel::onGuiRender() {
        if (!m_open)
            return;

        const std::string name = m_path.empty()
            ? std::string("JSON")
            : fileNameOf(m_path);
        const std::string title = "JSON - " + name + (hasUnsavedWork() ? " *" : "") + "###jsonEditor";

        ImGui::SetNextWindowSize(ImVec2(760.0f, 720.0f), ImGuiCond_FirstUseEver);
        const bool began = ImGui::Begin(title.c_str(), &m_open);

        if (!began) {
            // The X button cleared m_open; with edits pending, keep the window
            // and ask instead of discarding them.
            if (!m_open && hasUnsavedWork()) {
                m_open = true;
                requestConfirm(ConfirmAction::Close);
            }
            ImGui::End();
            return;
        }

        if (ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows)
            && ImGui::GetIO().KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_S, false))
            saveFromUi();

        drawToolbar();

        if (m_parseFailed) {
            ImGui::TextColored(ImVec4(0.95f, 0.35f, 0.35f, 1.0f), "Load failed");
            ImGui::TextWrapped("%s", m_error.c_str());
            if (ImGui::Button("Reload"))
                loadFile(m_path);
        }
        else {
            drawTree();
        }

        drawConfirmPopup();
        ImGui::End();
    }

    void JsonEditorPanel::drawToolbar() {
        const bool dirty = m_doc.dirty();

        if (dirty) {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.20f, 0.55f, 0.25f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.26f, 0.66f, 0.32f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.16f, 0.45f, 0.20f, 1.0f));
        }
        if (ImGui::Button("Save"))
            saveFromUi();
        if (dirty)
            ImGui::PopStyleColor(3);

        ImGui::SameLine();
        if (ImGui::Button("Revert")) {
            if (hasUnsavedWork())
                requestConfirm(ConfirmAction::Revert);
            else
                loadFile(m_path);
        }

        ImGui::SameLine();
        ImGui::TextDisabled("%s", m_path.c_str());

        if (!m_status.empty()) {
            ImGui::SameLine();
            ImGui::TextColored(m_statusError ? ImVec4(0.95f, 0.35f, 0.35f, 1.0f)
                                             : ImVec4(0.60f, 0.60f, 0.62f, 1.0f),
                               "%s", m_status.c_str());
        }

        ImGui::Separator();
    }

    void JsonEditorPanel::drawTree() {
        ImGui::BeginChild("##jsonTree", ImVec2(0.0f, 0.0f), ImGuiChildFlags_None,
                          ImGuiWindowFlags_HorizontalScrollbar);
        drawNode(m_doc.root(), JsonNode(), 0, "$", true, 0);
        applyPending();
        ImGui::EndChild();
    }

    void JsonEditorPanel::drawNode(const JsonNode& node, const JsonNode& parent, size_t index,
                                   const std::string& label, bool isRoot, int depth) {
        if (!node.valid())
            return;
        if (depth > 64) {
            ImGui::TextDisabled("... (nesting is too deep to show)");
            return;
        }

        const std::string path = node.path();
        ImGui::PushID(path.c_str());

        const JsonKind kind = node.kind();
        const bool isContainer = kind == JsonKind::Object || kind == JsonKind::Array;
        const size_t count = node.childCount();
        const bool expandable = isContainer && count > 0;
        const bool editingKey = m_editIsKey && m_editPath == path;

        // The row itself is the hit box for right-click and drag, and the key
        // badge and value widgets are drawn on top of it. The row must therefore
        // allow overlap, or it would swallow the checkbox, the double-click and
        // the inline editor.
        ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_FramePadding
            | ImGuiTreeNodeFlags_SpanAvailWidth;
        if (isContainer) {
            if (expandable)
                flags |= ImGuiTreeNodeFlags_OpenOnArrow;
            else
                flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
        }
        else {
            flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
        }
        if (isRoot)
            flags |= ImGuiTreeNodeFlags_DefaultOpen;
        ImGui::SetNextItemAllowOverlap();

        if (isContainer) {
            ImGui::PushStyleColor(ImGuiCol_Text, kKeyColor);
            const bool open = ImGui::TreeNodeEx("##node", flags, "%s",
                                                editingKey ? "" : label.c_str());
            ImGui::PopStyleColor();

            drawRowInteractions(node, parent, index, isRoot);

            if (editingKey) {
                ImGui::SameLine();
                drawInlineEditor();
            }

            ImGui::SameLine();
            ImGui::TextColored(kindColor(kind), "%s  %zu", kindBadge(kind), count);

            if (expandable && open) {
                const size_t limit = childLimit(path, count);
                for (size_t i = 0; i < limit; ++i) {
                    const std::string childLabel = kind == JsonKind::Object
                        ? node.keyAt(i)
                        : ("[" + std::to_string(i) + "]");
                    drawNode(node.childAt(i), node, i, childLabel, false, depth + 1);
                }

                if (limit < count) {
                    if (ImGui::Button("Show more..."))
                        m_childLimit[path] = limit + 500;
                }

                ImGui::TreePop();
            }
        }
        else {
            ImGui::TreeNodeEx("##node", flags, "%s", "");
            drawRowInteractions(node, parent, index, isRoot);
            ImGui::SameLine();
            drawKeyLabel(node, parent, label);
            ImGui::SameLine();
            ImGui::TextColored(kindColor(kind), "%s", kindBadge(kind));
            ImGui::SameLine();
            drawValue(node);
        }

        ImGui::PopID();
    }

    void JsonEditorPanel::drawKeyLabel(const JsonNode& node, const JsonNode& parent,
                                       const std::string& label) {
        if (m_editIsKey && m_editPath == node.path()) {
            drawInlineEditor();
            return;
        }

        const bool isIndex = parent.valid() && parent.kind() == JsonKind::Array;
        ImGui::TextColored(isIndex ? kIndexColor : kKeyColor, "%s", label.c_str());
    }

    void JsonEditorPanel::drawValue(JsonNode node) {
        const std::string path = node.path();
        if (!m_editIsKey && m_editPath == path) {
            drawInlineEditor();
            return;
        }

        const JsonKind kind = node.kind();
        switch (kind) {
        case JsonKind::Bool: {
            bool value = node.asBool();
            ImGui::PushStyleColor(ImGuiCol_CheckMark, kindColor(JsonKind::Bool));
            if (ImGui::Checkbox("##bool", &value))
                node.setBool(value);
            ImGui::PopStyleColor();
            break;
        }
        case JsonKind::Null:
            ImGui::TextDisabled("null");
            break;
        case JsonKind::String: {
            std::string text = node.asString();
            for (char& c : text)
                if (c == '\n' || c == '\r' || c == '\t')
                    c = ' ';
            ImGui::TextColored(kindColor(kind), "\"%s\"", text.c_str());
            if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
                beginEdit(node, JsonNode(), 0, node.asString(), false);
            break;
        }
        case JsonKind::Integer:
        case JsonKind::Float: {
            const std::string text = node.asRawText();
            ImGui::TextColored(kindColor(kind), "%s", text.c_str());
            if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
                beginEdit(node, JsonNode(), 0, text, false);
            break;
        }
        default:
            break;
        }
    }

    void JsonEditorPanel::drawRowInteractions(const JsonNode& node, const JsonNode& parent,
                                              size_t index, bool isRoot) {
        if (!isRoot) {
            // Drag and drop reorders inside the same container only; the target
            // index is the row under the cursor.
            const size_t from = index;
            if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceNoDisableHover)) {
                ImGui::SetDragDropPayload("VE_JSON_ROW", &from, sizeof(from));
                m_dragParent = parent.path();
                ImGui::TextUnformatted("Move");
                ImGui::EndDragDropSource();
            }

            if (ImGui::BeginDragDropTarget()) {
                if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("VE_JSON_ROW")) {
                    if (payload->DataSize == (int)sizeof(size_t)) {
                        const size_t src = *static_cast<const size_t*>(payload->Data);
                        if (m_dragParent == parent.path() && src != index) {
                            PendingOp op;
                            op.type = PendingOp::Type::Move;
                            op.container = parent;
                            op.index = src;
                            op.toIndex = index;
                            queue(std::move(op));
                        }
                    }
                }
                ImGui::EndDragDropTarget();
            }
        }

        if (!ImGui::BeginPopupContextItem("##rowMenu"))
            return;

        const JsonKind kind = node.kind();

        if (kind == JsonKind::Object) {
            if (ImGui::MenuItem("Add key")) {
                PendingOp op;
                op.type = PendingOp::Type::AddMember;
                op.container = node;
                op.text = uniqueKey(node);
                queue(std::move(op));
            }
        }
        if (kind == JsonKind::Array) {
            if (ImGui::MenuItem("Add element")) {
                PendingOp op;
                op.type = PendingOp::Type::AddElement;
                op.container = node;
                op.kind = JsonKind::Null;
                queue(std::move(op));
            }
        }

        if (ImGui::BeginMenu("Change type")) {
            const JsonKind kinds[] = { JsonKind::Null, JsonKind::Bool, JsonKind::Integer,
                                       JsonKind::Float, JsonKind::String, JsonKind::Object,
                                       JsonKind::Array };
            for (JsonKind target : kinds) {
                if (target == kind)
                    continue;
                if (ImGui::MenuItem(kindName(target))) {
                    PendingOp op;
                    op.type = PendingOp::Type::ChangeKind;
                    op.target = node;
                    op.kind = target;
                    queue(std::move(op));
                }
            }
            ImGui::EndMenu();
        }

        if (!isRoot) {
            if (parent.valid() && parent.kind() == JsonKind::Object) {
                if (ImGui::MenuItem("Rename key"))
                    beginEdit(node, parent, index, node.keyAt(index), true);
            }
            if (ImGui::MenuItem("Remove")) {
                PendingOp op;
                op.type = PendingOp::Type::Remove;
                op.container = parent;
                op.index = index;
                queue(std::move(op));
            }
        }

        ImGui::EndPopup();
    }

    void JsonEditorPanel::drawInlineEditor() {
        if (m_editFocus) {
            ImGui::SetKeyboardFocusHere();
            m_editFocus = false;
        }

        ImGui::SetNextItemWidth(240.0f);
        const bool committed = ImGui::InputText(
            "##inlineEdit", m_editBuffer.data(), m_editBuffer.capacity() + 1,
            ImGuiInputTextFlags_CallbackResize | ImGuiInputTextFlags_EnterReturnsTrue
                | ImGuiInputTextFlags_AutoSelectAll,
            textResizeCallback, &m_editBuffer);

        if (committed || ImGui::IsItemDeactivated())
            commitEdit();
    }

    void JsonEditorPanel::beginEdit(const JsonNode& node, const JsonNode& parent, size_t index,
                                    const std::string& text, bool isKey) {
        m_editPath = node.path();
        m_editBuffer = text;
        m_editIsKey = isKey;
        m_editFocus = true;
        m_editNode = node;
        m_editParent = parent;
        m_editIndex = index;
    }

    void JsonEditorPanel::endEdit() {
        m_editPath.clear();
        m_editBuffer.clear();
        m_editIsKey = false;
        m_editFocus = false;
        m_editNode = JsonNode();
        m_editParent = JsonNode();
        m_editIndex = 0;
    }

    void JsonEditorPanel::commitEdit() {
        if (m_editPath.empty())
            return;

        bool ok = true;

        if (m_editIsKey) {
            if (m_editParent.valid() && m_editIndex < m_editParent.childCount()) {
                if (m_editBuffer.empty()) {
                    setStatus("A key cannot be empty.", true);
                    ok = false;
                }
                else if (!m_editParent.renameKey(m_editIndex, m_editBuffer)) {
                    setStatus("That key already exists.", true);
                    ok = false;
                }
            }
        }
        else if (m_editNode.valid()) {
            switch (m_editNode.kind()) {
            case JsonKind::Integer: {
                errno = 0;
                char* end = nullptr;
                const long long value = std::strtoll(m_editBuffer.c_str(), &end, 10);
                if (end == m_editBuffer.c_str() || *end != '\0' || errno == ERANGE) {
                    setStatus("Not a valid 64-bit integer.", true);
                    ok = false;
                }
                else {
                    m_editNode.setInteger(static_cast<int64_t>(value));
                }
                break;
            }
            case JsonKind::Float: {
                errno = 0;
                char* end = nullptr;
                const double value = std::strtod(m_editBuffer.c_str(), &end);
                if (end == m_editBuffer.c_str() || *end != '\0'
                    || errno == ERANGE || !std::isfinite(value)) {
                    setStatus("Not a valid number.", true);
                    ok = false;
                }
                else if (!m_editNode.setFloat(value)) {
                    setStatus("Not a finite number.", true);
                    ok = false;
                }
                break;
            }
            case JsonKind::String:
                m_editNode.setString(m_editBuffer);
                break;
            default:
                break;
            }
        }

        if (ok) {
            if (m_statusError)
                m_status.clear();
            m_statusError = false;
            endEdit();
        }
    }

    void JsonEditorPanel::queue(PendingOp op) {
        m_pending = std::move(op);
    }

    void JsonEditorPanel::applyPending() {
        if (m_pending.type == PendingOp::Type::None)
            return;

        PendingOp op = std::move(m_pending);
        m_pending = PendingOp();

        switch (op.type) {
        case PendingOp::Type::AddMember: {
            if (op.container.appendMember(op.text, JsonKind::Null)) {
                const size_t added = op.container.childCount() - 1;
                JsonNode child = op.container.childAt(added);
                // Let the generated name be typed over straight away.
                beginEdit(child, op.container, added, op.text, true);
                setStatus("Type a key name, then press Enter.", false);
            }
            break;
        }
        case PendingOp::Type::AddElement:
            endEdit();
            op.container.appendElement(op.kind);
            break;
        case PendingOp::Type::Remove:
            endEdit();
            op.container.removeChild(op.index);
            break;
        case PendingOp::Type::ChangeKind:
            endEdit();
            if (op.target.valid())
                op.target.setKind(op.kind);
            break;
        case PendingOp::Type::Move:
            endEdit();
            op.container.moveChild(op.index, op.toIndex);
            break;
        default:
            break;
        }
    }

    size_t JsonEditorPanel::childLimit(const std::string& path, size_t count) const {
        const size_t step = 500;
        auto it = m_childLimit.find(path);
        const size_t limit = it == m_childLimit.end() ? step : it->second;
        return std::min(limit, count);
    }

    void JsonEditorPanel::setStatus(const std::string& text, bool error) {
        m_status = text;
        m_statusError = error;
    }

}
