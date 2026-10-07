#pragma once

#include <string>
#include <filesystem>
#include <vector>
#include <functional>
#include <unordered_map>

#include "Renderer/Texture.h"

namespace ve {

    class FileBrowser {
    public:
        FileBrowser();
        ~FileBrowser();

        void loadIcons();

        std::shared_ptr<Texture2D> getIconForFile(const std::string& path, bool isDirectory);

        void setRootPath(const std::string& path);
        void render();

        using FileSelectCallback = std::function<void(const std::string& path)>;
        void setOnFileSelect(FileSelectCallback callback);

    private:
        void refreshFiles();
        void pasteClipboard();

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
        FileSelectCallback m_onFileSelect;
        std::string m_selectedPath;

        std::shared_ptr<Texture2D> m_folderIcon;
        std::shared_ptr<Texture2D> m_shaderIcon;
        std::shared_ptr<Texture2D> m_sourceIcon;
        std::shared_ptr<Texture2D> m_materialIcon;
        std::shared_ptr<Texture2D> m_staticMeshIcon;
        std::shared_ptr<Texture2D> m_terrainIcon;
        std::shared_ptr<Texture2D> m_notImportedIcon;

        // path -> .veasset type token. Refilled on refreshFiles(); without it
        // getIconForFile() would hit the disk for every asset every frame.
        std::unordered_map<std::string, std::string> m_assetTypeCache;

        // "<token>/<stem>" -> baked asset path, for sources in the shown
        // directory. A source counts as imported when its kind + stem map to an
        // entry here.
        std::unordered_map<std::string, std::string> m_bakedAssetIndex;

        bool m_showNewMaterialPopup = false;
        char m_newMaterialName[256] = {};

        // Right-clicked item, shown in the item context menu.
        std::string m_contextPath;
        std::string m_contextName;
        bool m_contextIsDirectory = false;

        // In-app clipboard: "Copy" on an item, "Paste" in an empty area copies
        // the file into the currently shown directory.
        std::string m_clipboardPath;

        std::string m_pendingDeletePath;
        std::string m_pendingDeleteName;
        bool m_openDeleteConfirm = false;
        bool m_showDeleteConfirm = false;
    };

}