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
        std::shared_ptr<Texture2D> m_imageIcon;
        std::shared_ptr<Texture2D> m_materialIcon;
        std::shared_ptr<Texture2D> m_defaultIcon;

        std::unordered_map<std::string, std::shared_ptr<Texture2D>> m_textureCache;
        std::unordered_map<std::string, std::shared_ptr<Texture2D>> m_materialThumbCache;

        bool m_showNewMaterialPopup = false;
        char m_newMaterialName[256] = {};
    };

}