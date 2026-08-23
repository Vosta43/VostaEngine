#include "FileBrowser.h"
#include "Renderer/Texture.h"
#include "Asset/ImportManager.h"
#include "Platform/OpenGL/OpenGLTexture.h"
#include "Asset/Utils.h"
#include "Core/Log.h"
#include "Core/ResourceManager.h"
#include "Renderer/Material.h"
#include "Renderer/ThumbnailRenderer.h"
#include "Scene/Archive.h"
#include <imgui.h>
#include <windows.h>
#include <commdlg.h>

//TODO: This is Windows only function
static std::string openFileDialogWindows() {
    OPENFILENAMEA ofn = {};
    char szFile[1024] = {};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = GetActiveWindow();
    ofn.lpstrFile = szFile;
    ofn.nMaxFile = sizeof(szFile);
    ofn.lpstrFilter = "All\0*.*\0Textures\0*.png;*.jpg;*.hdr\0Models\0*.obj;*.fbx\0";
    ofn.nFilterIndex = 1;
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;

    if (GetOpenFileNameA(&ofn)) {
        return ofn.lpstrFile;
    }
    return {};
}


namespace ve {

    FileBrowser::FileBrowser()
        : m_rootPath(".") {
        loadIcons();
        refreshFiles();
    }

    FileBrowser::~FileBrowser() {};

    void FileBrowser::setRootPath(const std::string& path) {
        m_rootPath = path;
        m_currentPath = path;
        refreshFiles();
    }

    void FileBrowser::refreshFiles() {
        m_files.clear();

        if (!std::filesystem::exists(m_currentPath))
            return;

        if (m_currentPath != m_rootPath) {
            FileEntry parent;
            parent.name = "..";
            parent.path = std::filesystem::path(m_currentPath).parent_path().string();
            parent.isDirectory = true;
            m_files.push_back(parent);
        }

        for (const auto& entry : std::filesystem::directory_iterator(m_currentPath)) {
            FileEntry e;
            e.name = entry.path().filename().string();
            e.path = entry.path().string();
            e.isDirectory = entry.is_directory();
            m_files.push_back(e);
        }
    }

    void FileBrowser::loadIcons() {
        m_folderIcon = std::shared_ptr<Texture2D>(Texture2D::create("VostaEngine/resources/icons/folder.png"));
        m_shaderIcon = std::shared_ptr<Texture2D>(Texture2D::create("VostaEngine/resources/icons/shader.png"));
        m_sourceIcon = std::shared_ptr<Texture2D>(Texture2D::create("VostaEngine/resources/icons/source_code.png"));
        m_imageIcon = std::shared_ptr<Texture2D>(Texture2D::create("VostaEngine/resources/icons/image.png"));
        m_materialIcon = std::shared_ptr<Texture2D>(Texture2D::create("VostaEngine/resources/icons/material.png"));
        m_defaultIcon = std::shared_ptr<Texture2D>(Texture2D::create("VostaEngine/resources/icons/default.png"));
    }

    // Read the first token from a .veasset file to determine its asset type.
    static std::string peekAssetType(const std::string& path) {
        TextArchive ar(path, ArchiveMode::read);
        if (!ar.isGood()) return {};
        std::string type;
        ar >> type;
        return type;
    }

    std::shared_ptr<Texture2D> FileBrowser::getIconForFile(const std::string& path, bool isDirectory) {
        if (isDirectory)
            return m_folderIcon;

        std::string ext = ve::utils::getExtension(path);

        if (ext == ".cpp" || ext == ".h" || ext == ".hpp" || ext == ".c")
            return m_sourceIcon;
        if (ext == ".glsl" || ext == ".vert" || ext == ".frag" || ext == ".comp")
            return m_shaderIcon;
        if (ext == ".png" || ext == ".jpg" || ext == ".hdr") {
            auto it = m_textureCache.find(path);
            if (it != m_textureCache.end()) {
                return it->second;
            }
            auto texture = Texture2D::create(path);
            if (texture) {
                m_textureCache[path] = texture;
                return texture;
            }
            else {
                return m_imageIcon;
            }
        }

        if (ext == ".veasset") {
            std::string assetType = peekAssetType(path);
            if (assetType == "material") {
                // Try material thumbnail cache first.
                auto it = m_materialThumbCache.find(path);
                if (it != m_materialThumbCache.end() && it->second) {
                    return it->second;
                }

                // Load the material and get its preview thumbnail.
                AssetHandle handle = ResourceManager::find<Material>(path);
                if (!handle.isValid()) {
                    handle = ResourceManager::store<Material>(path);
                }
                if (handle.isValid()) {
                    Ref<Texture2D> thumb = ThumbnailRenderer::getMaterialThumbnail(handle);
                    if (thumb) {
                        m_materialThumbCache[path] = thumb;
                        return thumb;
                    }
                }
                return m_materialIcon;
            }
            // Unknown veasset type — fall through to default.
        }

        return m_defaultIcon;
    }

    void FileBrowser::render() {
        ImGui::Begin("File Browser");

        if (ImGui::Button("Import")) {
            std::string filePath = openFileDialogWindows();
            ImportManager::import(filePath);
        }

        ImGui::Text("Path: %s", m_currentPath.c_str());
        ImGui::Separator();

        float iconSize = 64.0f;
        float textHeight = ImGui::GetTextLineHeight();
        float cellWidth = iconSize + 16.0f;
        float cellHeight = iconSize + textHeight + 12.0f;

        int columns = (int)(ImGui::GetContentRegionAvail().x / cellWidth);
        if (columns < 1) columns = 1;

        ImGui::Columns(columns, nullptr, false);

        bool rightClickedEmpty = false;
        ImVec2 regionMin = ImGui::GetWindowContentRegionMin();
        ImVec2 regionMax = ImGui::GetWindowContentRegionMax();
        ImVec2 mousePos = ImGui::GetMousePos();
        if (ImGui::IsMouseClicked(ImGuiMouseButton_Right) && ImGui::IsWindowHovered()) {
            bool clickedOnItem = false;
            for (const auto& file : m_files) {
                if (ImGui::IsItemHovered()) {
                    clickedOnItem = true;
                    break;
                }
            }
            if (!clickedOnItem) {
                rightClickedEmpty = true;
                ImGui::OpenPopup("EmptyAreaContextMenu");
            }
        }

        for (const auto& file : m_files) {
            auto icon = getIconForFile(file.path, file.isDirectory);

            ImGui::BeginGroup();

            ImVec2 pos = ImGui::GetCursorScreenPos();
            ImVec2 size(iconSize, iconSize);

            if (icon && icon->getRendererID()) {
                ImGui::GetWindowDrawList()->AddImage(
                    (ImTextureID)(uintptr_t)icon->getRendererID(),
                    pos,
                    ImVec2(pos.x + iconSize, pos.y + iconSize),
                    ImVec2(0, 1), ImVec2(1, 0)
                );
            }
            else {
                ImGui::GetWindowDrawList()->AddRectFilled(
                    pos,
                    ImVec2(pos.x + iconSize, pos.y + iconSize),
                    IM_COL32(60, 60, 60, 255)
                );
            }

            ImGui::Dummy(size);

            if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(0)) {
                if (file.isDirectory) {
                    m_currentPath = file.path;
                    refreshFiles();
                }
                else {
                    if (m_onFileSelect) {
                        m_onFileSelect(file.path);
                    }
                }
            }

            if (ImGui::IsItemClicked(ImGuiMouseButton_Left)) {
                m_selectedPath = file.path;
            }

            if (ImGui::IsItemHovered()) {
                ImGui::GetWindowDrawList()->AddRectFilled(
                    pos,
                    ImVec2(pos.x + iconSize, pos.y + iconSize),
                    IM_COL32(255, 255, 255, 50)
                );
            }

            std::string displayName = file.name;
            float maxWidth = cellWidth - 8.0f;
            if (ImGui::CalcTextSize(displayName.c_str()).x > maxWidth) {
                displayName = displayName.substr(0, 8) + "..";
            }
            ImGui::SetCursorPosX(ImGui::GetCursorPosX() + (cellWidth - ImGui::CalcTextSize(displayName.c_str()).x) * 0.5f);
            ImGui::Text("%s", displayName.c_str());

            ImGui::EndGroup();
            ImGui::NextColumn();
        }

        ImGui::Columns(1);

        // --- Context menu ---
        if (ImGui::BeginPopup("EmptyAreaContextMenu")) {

            if (ImGui::MenuItem("New Folder")) {

            }
            else if (ImGui::MenuItem("Material")) {
                m_showNewMaterialPopup = true;
                m_newMaterialName[0] = '\0';
            }
            else if (ImGui::MenuItem("Refresh")) {
                refreshFiles();
            }
            ImGui::EndPopup();
        }

        // --- New Material name popup ---
        if (m_showNewMaterialPopup) {
            ImGui::OpenPopup("New Material");
        }
        if (ImGui::BeginPopupModal("New Material", &m_showNewMaterialPopup, ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::Text("Enter material name:");
            ImGui::InputText("##name", m_newMaterialName, sizeof(m_newMaterialName));
            if (ImGui::Button("Create", ImVec2(120, 0))) {
                std::string name(m_newMaterialName);
                if (!name.empty()) {
                    if (name.find(".veasset") == std::string::npos) {
                        name += ".veasset";
                    }
                    std::string fullPath = m_currentPath + "/" + name;
                    ResourceManager::store<Material>(fullPath);
                    refreshFiles();
                }
                m_showNewMaterialPopup = false;
            }
            ImGui::SameLine();
            if (ImGui::Button("Cancel", ImVec2(120, 0))) {
                m_showNewMaterialPopup = false;
            }
            ImGui::EndPopup();
        }

        ImGui::End();
    }

}
