#pragma once

#include <string>
#include <filesystem>
#include <vector>
#include <functional>
#include <unordered_map>

#include "Renderer/Texture.h"

namespace ve {

    // Win32 shell dialogs. Defined in FileBrowser.cpp, which is the editor's
    // single home for <windows.h>. Both return "" when the user cancels.
    std::string openImportFileDialog();
    std::string pickFolderDialog(const std::string& initial);

    class FileBrowser {
    public:
        FileBrowser();
        ~FileBrowser();

        void loadIcons();

        std::shared_ptr<Texture2D> getIconForFile(const std::string& path, bool isDirectory);

        void setRootPath(const std::string& path);
        void render();

        // Ask for a rescan on the next render(). Callers that change assets
        // elsewhere (AI writes, scripts) use this instead of refreshing now: the
        // listing stays owned by render(), and an unrendered panel costs nothing.
        void requestRefresh();

        using FileSelectCallback = std::function<void(const std::string& path)>;
        void setOnFileSelect(FileSelectCallback callback);

        // Scene-specific action from the item context menu ("Set as Default
        // Scene"). The panel only knows paths; the host owns what "default"
        // means and where it persists.
        using SetDefaultSceneCallback = std::function<void(const std::string& path)>;
        void setOnSetDefaultScene(SetDefaultSceneCallback callback);

        // Absolute path of the current default scene, so its menu item shows a
        // check mark. Empty when the project has none.
        void setDefaultScenePath(const std::string& path);

    private:
        void refreshFiles();
        void pasteClipboard();

        // Inline rename: beginRename() makes the item's name cell an input box
        // (selecting the whole name); commitRename() applies or discards it.
        void beginRename(const std::string& path, const std::string& initialName);
        void commitRename();

        // Baked .veasset for a source file, or "" if none. Matches on asset
        // kind + stem so a "_1"-suffixed bake is still found.
        std::string findBaked(const std::string& sourcePath, const char* token) const;

        struct FileEntry {
            std::string name;
            std::string path;
            bool isDirectory;
        };

        std::string m_rootPath;
        std::string m_currentPath;
        std::vector<FileEntry> m_files;
        // Sort the listing by asset kind (extension), folders first. Off keeps
        // the directory's natural order.
        bool m_sortByType = false;
        bool m_refreshRequested = false;
        FileSelectCallback m_onFileSelect;
        SetDefaultSceneCallback m_onSetDefaultScene;
        std::string m_defaultScenePath;
        std::string m_selectedPath;

        std::shared_ptr<Texture2D> m_folderIcon;
        std::shared_ptr<Texture2D> m_shaderIcon;
        std::shared_ptr<Texture2D> m_sourceIcon;
        std::shared_ptr<Texture2D> m_materialIcon;
        std::shared_ptr<Texture2D> m_staticMeshIcon;
        std::shared_ptr<Texture2D> m_terrainIcon;
        std::shared_ptr<Texture2D> m_noiseIcon;
        std::shared_ptr<Texture2D> m_terrainDataIcon;
        std::shared_ptr<Texture2D> m_notImportedIcon;

        // path -> .veasset type token. Refilled on refreshFiles(); without it
        // getIconForFile() would hit the disk for every asset every frame.
        std::unordered_map<std::string, std::string> m_assetTypeCache;

        // "<token>/<stem>" -> baked asset path, for sources in the shown
        // directory. A source counts as imported when its kind + stem map to an
        // entry here.
        std::unordered_map<std::string, std::string> m_bakedAssetIndex;

        // Right-clicked item, shown in the item context menu.
        std::string m_contextPath;
        std::string m_contextName;
        bool m_contextIsDirectory = false;

        // Inline rename: when set, that item's name cell becomes an input box.
        // m_renameFocus requests focus + select-all on the first frame.
        std::string m_renamingPath;
        char m_renameBuffer[256] = {};
        bool m_renameFocus = false;

        // In-app clipboard: "Copy" on an item, "Paste" in an empty area copies
        // the file into the currently shown directory.
        std::string m_clipboardPath;

        std::string m_pendingDeletePath;
        std::string m_pendingDeleteName;
        bool m_openDeleteConfirm = false;
        bool m_showDeleteConfirm = false;
    };

}