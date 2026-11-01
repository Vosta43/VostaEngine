#pragma once

#include <VostaEngine.h>

#include "Core/Json.h"

#include <cstddef>
#include <string>
#include <unordered_map>

namespace ve {

// Visual editor for a plain .json file: a colored tree whose scalars are edited
// in place, with add / remove / rename / reorder for keys and elements. JSON is
// not an asset kind, so this reads and writes the file directly and registers
// nothing with ResourceManager or AssetLibrary.
class JsonEditorPanel {
public:
    // Opens `path`. With unsaved edits in another file, asks before replacing.
    void openFile(const std::string& path);

    void onGuiRender();

private:
    enum class ConfirmAction { None, Close, Revert, OpenOther };

    // Tree edits (add / remove / move / change type) are queued during the walk
    // and applied after it, so the walk never sees indices shift under it.
    struct PendingOp {
        enum class Type { None, AddMember, AddElement, Remove, ChangeKind, Move } type = Type::None;
        JsonNode    container;   // object/array that gains or loses a child
        JsonNode    target;      // node whose own value is replaced
        size_t      index = 0;
        size_t      toIndex = 0;
        std::string text;
        JsonKind    kind = JsonKind::Null;
    };

    void loadFile(const std::string& path);
    // A value typed into the inline field but not yet committed is unsaved too.
    bool hasUnsavedWork() const;
    // Commits the active inline field (if any) first, so Save never writes the
    // value that was on screen before the user's last keystrokes.
    void saveFromUi();
    void saveNow();
    void applyAction(ConfirmAction action);
    void requestConfirm(ConfirmAction action);
    void drawConfirmPopup();

    void drawToolbar();
    void drawTree();
    void drawNode(const JsonNode& node, const JsonNode& parent, size_t index,
                  const std::string& label, bool isRoot, int depth);
    void drawRowInteractions(const JsonNode& node, const JsonNode& parent,
                             size_t index, bool isRoot);
    void drawKeyLabel(const JsonNode& node, const JsonNode& parent,
                      const std::string& label);
    // By value: JsonNode is a cheap handle, and the bool checkbox writes
    // through the handle to the document.
    void drawValue(JsonNode node);
    void drawInlineEditor();

    void beginEdit(const JsonNode& node, const JsonNode& parent, size_t index,
                   const std::string& text, bool isKey);
    void endEdit();
    void commitEdit();

    void queue(PendingOp op);
    void applyPending();

    size_t childLimit(const std::string& path, size_t count) const;
    void setStatus(const std::string& text, bool error);

    JsonDocument m_doc;
    std::string  m_path;
    bool         m_open = false;
    bool         m_parseFailed = false;
    std::string  m_error;
    std::string  m_status;
    bool         m_statusError = false;

    std::string m_editPath;
    std::string m_editBuffer;
    bool        m_editIsKey = false;
    bool        m_editFocus = false;
    JsonNode    m_editNode;
    JsonNode    m_editParent;
    size_t      m_editIndex = 0;

    PendingOp   m_pending;
    std::string m_dragParent;
    std::unordered_map<std::string, size_t> m_childLimit;

    ConfirmAction m_confirm = ConfirmAction::None;
    bool          m_confirmRequested = false;
    std::string   m_pendingOpenPath;
};

}
