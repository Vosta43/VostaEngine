#include "FileBrowser.h"
#include "Renderer/Texture.h"
#include "Renderer/StaticMesh.h"
#include "Asset/ImportManager.h"
#include "Platform/OpenGL/OpenGLTexture.h"
#include "Asset/Utils.h"
#include "Core/Log.h"
#include "Core/ResourceManager.h"
#include "Renderer/Material.h"
#include "Renderer/ThumbnailRenderer.h"
#include <imgui.h>
#include <windows.h>
#include <commdlg.h>
#include <fstream>

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

    // Defined below; used by refreshFiles() and findBaked().
    static std::string normalizedStem(const std::string& path);

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
        m_assetTypeCache.clear();
        m_bakedAssetIndex.clear();

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

        // Index the baked assets once, so a source file can find its bake by
        // kind + stem without re-deriving a name the auto-suffix may have moved.
        for (const auto& file : m_files) {
            if (file.isDirectory || ve::utils::getExtension(file.path) != ".veasset")
                continue;

            const std::string type = ve::utils::peekAssetToken(file.path);
            m_assetTypeCache[file.path] = type;
            if (type != "texture" && type != "staticmesh")
                continue;

            const std::string stem = std::filesystem::path(file.path).stem().string();
            const std::string key = type + "/" + normalizedStem(file.path);
            auto it = m_bakedAssetIndex.find(key);
            // Prefer the un-suffixed bake when a stem has both.
            if (it == m_bakedAssetIndex.end() || stem == normalizedStem(file.path))
                m_bakedAssetIndex[key] = file.path;
        }
    }

    std::string FileBrowser::findBaked(const std::string& sourcePath, const char* token) const {
        auto it = m_bakedAssetIndex.find(std::string(token) + "/" + normalizedStem(sourcePath));
        return it == m_bakedAssetIndex.end() ? std::string() : it->second;
    }

    void FileBrowser::pasteClipboard() {
        if (m_clipboardPath.empty())
            return;

        std::filesystem::path src(m_clipboardPath);
        std::error_code ec;
        if (!std::filesystem::exists(src, ec))
            return;

        std::filesystem::path destDir(m_currentPath);
        std::filesystem::path dest = destDir / src.filename();

        // Don't clobber an existing entry: append " (n)" until the name is free.
        if (std::filesystem::exists(dest, ec)) {
            std::string stem = src.stem().string();
            std::string ext = src.extension().string();
            for (int i = 1; std::filesystem::exists(dest, ec); ++i) {
                dest = destDir / (stem + " (" + std::to_string(i) + ")" + ext);
            }
        }

        if (std::filesystem::is_directory(src, ec)) {
            std::filesystem::copy(src, dest,
                std::filesystem::copy_options::recursive, ec);
        }
        else {
            std::filesystem::copy_file(src, dest,
                std::filesystem::copy_options::overwrite_existing, ec);
        }

        if (ec)
            VE_CORE_WARN_PRINT("Paste failed: %s", dest.string().c_str());
        else
            VE_CORE_SUCCESS_PRINT("Pasted: %s", dest.string().c_str());

        refreshFiles();
    }

    void FileBrowser::loadIcons() {
        m_folderIcon = std::shared_ptr<Texture2D>(Texture2D::create("VostaEngine/resources/icons/folder.png"));
        m_shaderIcon = std::shared_ptr<Texture2D>(Texture2D::create("VostaEngine/resources/icons/shader.png"));
        m_sourceIcon = std::shared_ptr<Texture2D>(Texture2D::create("VostaEngine/resources/icons/source_code.png"));
        m_materialIcon = std::shared_ptr<Texture2D>(Texture2D::create("VostaEngine/resources/icons/material.png"));
        m_staticMeshIcon = std::shared_ptr<Texture2D>(Texture2D::create("VostaEngine/resources/icons/static_mesh.png"));
        m_terrainIcon = std::shared_ptr<Texture2D>(Texture2D::create("VostaEngine/resources/icons/terrain.png"));
        m_notImportedIcon = std::shared_ptr<Texture2D>(Texture2D::create("VostaEngine/resources/icons/not_imported_file.png"));
    }

    // Source types the importer can turn into project assets.
    static bool isImportableFile(const std::string& path) {
        const std::string ext = ve::utils::getExtension(path);
        return ext == ".obj" || ext == ".png" || ext == ".jpg" || ext == ".hdr";
    }

    // Stem with any trailing "_<digits>" auto-suffix removed, so "sofa_1" and
    // "sofa" both normalise to "sofa".
    static std::string normalizedStem(const std::string& path) {
        std::string stem = std::filesystem::path(path).stem().string();
        const auto pos = stem.find_last_of('_');
        if (pos != std::string::npos && pos + 1 < stem.size() &&
            stem.find_first_not_of("0123456789", pos + 1) == std::string::npos) {
            stem.erase(pos);
        }
        return stem;
    }

    // Hand the loaded texture straight to ImGui as its own icon. The
    // ResourceManager caches it by path, so re-opening a folder is a map lookup
    // rather than a re-decode.
    static std::shared_ptr<Texture2D> texturePreviewForAsset(const std::string& assetPath) {
        AssetHandle handle = ResourceManager::find<Texture2D>(assetPath);
        if (!handle.isValid())
            handle = ResourceManager::store<Texture2D>(assetPath);
        if (!handle.isValid())
            return nullptr;
        return ResourceManager::get<Texture2D>(handle);
    }

    // Colour of a tile's separator rule, by asset kind: folder white, mesh blue,
    // texture pink, shader purple, source dark blue. Other untyped assets keep
    // the default pale blue.
    static ImU32 tileAccentColor(const std::string& path, bool isDirectory,
                                 const std::unordered_map<std::string, std::string>& assetTypeCache) {
        const ImU32 kFolder  = IM_COL32(240, 240, 240, 255);
        const ImU32 kDefault = IM_COL32(130, 180, 235, 255);
        const ImU32 kMesh    = IM_COL32(70, 140, 245, 255);
        const ImU32 kTexture = IM_COL32(235, 125, 185, 255);
        const ImU32 kShader  = IM_COL32(170, 125, 240, 255);
        const ImU32 kSource  = IM_COL32(55, 90, 200, 255);

        if (isDirectory)
            return kFolder;

        const std::string ext = ve::utils::getExtension(path);
        if (ext == ".cpp" || ext == ".h" || ext == ".hpp" || ext == ".c") return kSource;
        if (ext == ".glsl" || ext == ".vert" || ext == ".frag" || ext == ".comp") return kShader;
        if (ext == ".obj" || ext == ".fbx") return kMesh;
        if (ext == ".png" || ext == ".jpg" || ext == ".hdr") return kTexture;

        if (ext == ".veasset") {
            auto it = assetTypeCache.find(path);
            const std::string type = it != assetTypeCache.end() ? it->second : ve::utils::peekAssetToken(path);
            if (type == "staticmesh") return kMesh;
            if (type == "texture")    return kTexture;
        }
        return kDefault;
    }

    // Wrap a name into at most two lines fitting maxWidth, breaking only on
    // UTF-8 character boundaries and ellipsising the second line if needed.
    static void wrapTwoLines(const std::string& text, float maxWidth,
                             std::string& line1, std::string& line2) {
        line1.clear();
        line2.clear();

        std::vector<std::string> glyphs;
        for (size_t i = 0; i < text.size();) {
            const unsigned char c = static_cast<unsigned char>(text[i]);
            size_t len = (c >= 0xF0) ? 4 : (c >= 0xE0) ? 3 : (c >= 0xC0) ? 2 : 1;
            if (len > text.size() - i) len = text.size() - i;
            glyphs.emplace_back(text.substr(i, len));
            i += len;
        }

        const auto fits = [maxWidth](const std::string& s) {
            return ImGui::CalcTextSize(s.c_str()).x <= maxWidth;
        };

        size_t i = 0;
        for (; i < glyphs.size(); ++i) {
            if (!fits(line1 + glyphs[i])) break;
            line1 += glyphs[i];
        }
        if (i >= glyphs.size())
            return;

        const std::string ellipsis = "\xE2\x80\xA6";   // U+2026
        for (; i < glyphs.size(); ++i) {
            if (!fits(line2 + glyphs[i] + ellipsis)) break;
            line2 += glyphs[i];
        }
        line2 += ellipsis;
    }

    std::shared_ptr<Texture2D> FileBrowser::getIconForFile(const std::string& path, bool isDirectory) {
        if (isDirectory)
            return m_folderIcon;

        std::string ext = ve::utils::getExtension(path);

        // Engine source, not project content.
        if (ext == ".cpp" || ext == ".h" || ext == ".hpp" || ext == ".c")
            return m_sourceIcon;
        if (ext == ".glsl" || ext == ".vert" || ext == ".frag" || ext == ".comp")
            return m_shaderIcon;

        // A model source counts as imported once its baked mesh asset exists.
        if (ext == ".obj" || ext == ".fbx") {
            return findBaked(path, "staticmesh").empty() ? m_notImportedIcon : m_staticMeshIcon;
        }
        if (ext == ".veworld")
            return m_terrainIcon;

        // An image counts as imported once its baked texture asset exists.
        if (ext == ".png" || ext == ".jpg" || ext == ".hdr") {
            const std::string baked = findBaked(path, "texture");
            if (baked.empty())
                return m_notImportedIcon;
            if (auto tex = texturePreviewForAsset(baked))
                return tex;
            return m_notImportedIcon;
        }

        if (ext == ".veasset") {
            auto typeIt = m_assetTypeCache.find(path);
            const std::string assetType = typeIt != m_assetTypeCache.end()
                ? typeIt->second : ve::utils::peekAssetToken(path);
            if (assetType == "texture") {
                if (auto tex = texturePreviewForAsset(path))
                    return tex;
                return m_notImportedIcon;
            }
            if (assetType == "material") {
                // ThumbnailRenderer owns the cache; a hit here is a map lookup.
                AssetHandle handle = ResourceManager::find<Material>(path);
                if (!handle.isValid()) {
                    handle = ResourceManager::store<Material>(path);
                }
                if (handle.isValid()) {
                    Ref<Texture2D> thumb = ThumbnailRenderer::getMaterialThumbnail(handle);
                    if (thumb) {
                        return thumb;
                    }
                }
                return m_materialIcon;
            }
            if (assetType == "staticmesh") {
                AssetHandle handle = ResourceManager::find<StaticMesh>(path);
                if (!handle.isValid()) {
                    handle = ResourceManager::store<StaticMesh>(path);
                }
                if (handle.isValid()) {
                    Ref<Texture2D> thumb = ThumbnailRenderer::getStaticMeshThumbnail(handle);
                    if (thumb) {
                        return thumb;
                    }
                }
                return m_staticMeshIcon;
            }
            // Unknown veasset type — fall through.
        }

        // Anything that is not a baked engine asset still has to be imported.
        return m_notImportedIcon;
    }

    void FileBrowser::render() {
        ImGui::Begin("File Browser");

        if (ImGui::Button("Import")) {
            std::string filePath = openFileDialogWindows();
            if (!filePath.empty()) {
                // Import into the directory the browser is currently showing.
                ImportManager::import(filePath, m_currentPath);
                refreshFiles();
            }
        }

        ImGui::Text("Path: %s", m_currentPath.c_str());
        ImGui::Separator();

        // A tile is one rounded card: thumbnail on top, a coloured rule, then up
        // to two centred lines of name.
        const float tile = 76.0f;
        const float pad = 8.0f;
        const float rounding = 6.0f;
        const float lineH = ImGui::GetTextLineHeight();
        const float spacingY = ImGui::GetStyle().ItemSpacing.y;
        const float textBlockH = lineH * 2.0f + spacingY;
        const float cellW = tile + pad * 2.0f;
        const float sepYOffset = pad + tile + pad;
        const float cellH = sepYOffset + pad + textBlockH + pad;

        int columns = (int)(ImGui::GetContentRegionAvail().x / cellW);
        if (columns < 1) columns = 1;

        ImGui::Columns(columns, nullptr, false);

        // An item that claims the right-click opens its own menu; only a click
        // no item took falls through to the empty-area menu below.
        bool openedItemPopup = false;
        // OpenPopup must run outside the per-item PushID scope so its hashed ID
        // matches the BeginPopup below; opening it here would include the item's
        // ID and the menu would never match.
        bool requestItemPopup = false;

        // Deferred so the loop never mutates m_files while iterating it.
        std::string navigateTo;
        std::string openFile;

        // t > 0 fades a colour toward white, t < 0 toward black.
        const auto shade = [](ImU32 col, float t) -> ImU32 {
            ImVec4 c = ImGui::ColorConvertU32ToFloat4(col);
            const float target = t < 0.0f ? 0.0f : 1.0f;
            const float k = t < 0.0f ? -t : t;
            c.x += (target - c.x) * k;
            c.y += (target - c.y) * k;
            c.z += (target - c.z) * k;
            return ImGui::GetColorU32(c);
        };

        for (const auto& file : m_files) {
            auto icon = getIconForFile(file.path, file.isDirectory);

            ImGui::PushID(file.path.c_str());

            const ImVec2 cellMin = ImGui::GetCursorScreenPos();
            const ImVec2 cellMax(cellMin.x + cellW, cellMin.y + cellH);

            ImGui::InvisibleButton("##cell", ImVec2(cellW, cellH));
            const bool hovered = ImGui::IsItemHovered();
            const bool selected = (m_selectedPath == file.path);

            if (hovered && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
                if (file.isDirectory) navigateTo = file.path;
                else openFile = file.path;
            }

            if (ImGui::IsItemClicked(ImGuiMouseButton_Left))
                m_selectedPath = file.path;

            if (ImGui::IsItemClicked(ImGuiMouseButton_Right)) {
                m_contextPath = file.path;
                m_contextName = file.name;
                m_contextIsDirectory = file.isDirectory;
                openedItemPopup = true;
                requestItemPopup = true;
            }

            ImDrawList* dl = ImGui::GetWindowDrawList();

            // The card itself is a flat fill.
            const ImU32 base = selected ? IM_COL32(58, 66, 84, 255)
                             : hovered  ? IM_COL32(68, 68, 68, 255)
                                        : IM_COL32(48, 48, 48, 255);
            dl->AddRectFilled(cellMin, cellMax, base, rounding);

            const ImU32 accent = tileAccentColor(file.path, file.isDirectory, m_assetTypeCache);

            // Only the name strip carries the vertical gradient: greyish-white at
            // the top fading down to darker. It sits on the card's rounded bottom,
            // so the strip is a full-width band plus a rounded bottom cap
            // (AddRectFilledMultiColor cannot round).
            const ImU32 stripTop = shade(base, 0.30f);
            const ImU32 stripBot = shade(base, -0.10f);
            if (cellMax.y - rounding > sepYOffset + cellMin.y) {
                dl->AddRectFilledMultiColor(
                    ImVec2(cellMin.x, cellMin.y + sepYOffset),
                    ImVec2(cellMax.x, cellMax.y - rounding),
                    stripTop, stripTop, stripBot, stripBot);
                dl->AddRectFilled(ImVec2(cellMin.x, cellMax.y - rounding), cellMax,
                                  stripBot, rounding, ImDrawFlags_RoundCornersBottom);
            }

            // Thumbnail: fit into the tile box preserving aspect, centred.
            const ImVec2 imgAreaMin(cellMin.x + pad, cellMin.y + pad);
            const ImVec2 imgAreaMax(imgAreaMin.x + tile, imgAreaMin.y + tile);

            if (icon && icon->getRendererID()) {
                const float tw = (float)icon->getWidth();
                const float th = (float)icon->getHeight();
                float scale = 1.0f;
                if (tw > 0.0f && th > 0.0f) {
                    const float sx = tile / tw, sy = tile / th;
                    scale = sx < sy ? sx : sy;
                }
                const ImVec2 imgSize(tw * scale, th * scale);
                const ImVec2 imgMin(imgAreaMin.x + (tile - imgSize.x) * 0.5f,
                                    imgAreaMin.y + (tile - imgSize.y) * 0.5f);
                dl->AddImage((ImTextureID)(uintptr_t)icon->getRendererID(),
                             imgMin, ImVec2(imgMin.x + imgSize.x, imgMin.y + imgSize.y),
                             ImVec2(0, 1), ImVec2(1, 0));
            }
            else {
                dl->AddRectFilled(imgAreaMin, imgAreaMax, IM_COL32(60, 60, 60, 255), 4.0f);
            }

            // Rule between the thumbnail and the name, tinted by asset kind.
            const float sepY = cellMin.y + sepYOffset;
            dl->AddLine(ImVec2(cellMin.x + pad, sepY), ImVec2(cellMax.x - pad, sepY),
                        accent, 1.5f);

            std::string line1, line2;
            wrapTwoLines(file.name, cellW - pad * 2.0f, line1, line2);

            const ImU32 textCol = IM_COL32(225, 225, 225, 255);
            const float textTop = sepY + pad;
            const float w1 = ImGui::CalcTextSize(line1.c_str()).x;
            dl->AddText(ImVec2(cellMin.x + (cellW - w1) * 0.5f, textTop), textCol, line1.c_str());
            if (!line2.empty()) {
                const float w2 = ImGui::CalcTextSize(line2.c_str()).x;
                dl->AddText(ImVec2(cellMin.x + (cellW - w2) * 0.5f, textTop + lineH + spacingY),
                            textCol, line2.c_str());
            }

            if (selected)
                dl->AddRect(cellMin, cellMax, ImGui::GetColorU32(ImGuiCol_ButtonActive),
                            rounding, 0, 2.0f);

            ImGui::PopID();
            ImGui::NextColumn();
        }

        ImGui::Columns(1);

        if (!openFile.empty() && m_onFileSelect)
            m_onFileSelect(openFile);
        if (!navigateTo.empty()) {
            m_currentPath = navigateTo;
            refreshFiles();
        }

        // --- Item context menu ---
        if (requestItemPopup) {
            ImGui::OpenPopup("ItemContextMenu");
        }
        if (ImGui::BeginPopup("ItemContextMenu")) {
            // Full name, since the cell label truncates long ones.
            ImGui::TextDisabled("%s", m_contextName.c_str());
            ImGui::Separator();

            // Only for raw sources the importer understands; a directory or an
            // already-baked .veasset has nothing to import.
            if (ImGui::MenuItem("Import", nullptr, false,
                    !m_contextIsDirectory && isImportableFile(m_contextPath))) {
                ImportManager::import(m_contextPath, m_currentPath);
                refreshFiles();
            }
            if (ImGui::MenuItem("Copy")) {
                m_clipboardPath = m_contextPath;
            }
            if (ImGui::MenuItem("Delete")) {
                m_pendingDeletePath = m_contextPath;
                m_pendingDeleteName = m_contextName;
                m_openDeleteConfirm = true;
            }
            ImGui::EndPopup();
        }

        // --- Empty-area context menu ---
        if (ImGui::IsMouseClicked(ImGuiMouseButton_Right) && ImGui::IsWindowHovered() && !openedItemPopup) {
            ImGui::OpenPopup("EmptyAreaContextMenu");
        }
        if (ImGui::BeginPopup("EmptyAreaContextMenu")) {

            if (ImGui::MenuItem("New Folder")) {
                std::error_code ec;
                std::filesystem::path dir(m_currentPath + "/New Folder");
                for (int i = 1; std::filesystem::exists(dir, ec); ++i)
                    dir = std::filesystem::path(m_currentPath + "/New Folder " + std::to_string(i));
                std::filesystem::create_directories(dir, ec);
                if (ec)
                    VE_CORE_WARN_PRINT("New Folder failed: %s", dir.string().c_str());
                refreshFiles();
            }
            else if (ImGui::MenuItem("Material")) {
                m_showNewMaterialPopup = true;
                m_newMaterialName[0] = '\0';
            }
            else if (ImGui::MenuItem("Paste", nullptr, false, !m_clipboardPath.empty())) {
                pasteClipboard();
            }
            else if (ImGui::MenuItem("Refresh")) {
                refreshFiles();
            }
            ImGui::EndPopup();
        }

        // --- Delete confirmation ---
        if (m_openDeleteConfirm) {
            ImGui::OpenPopup("Delete");
            m_openDeleteConfirm = false;
            m_showDeleteConfirm = true;
        }
        if (ImGui::BeginPopupModal("Delete", &m_showDeleteConfirm, ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::Text("Delete \"%s\"?", m_pendingDeleteName.c_str());
            ImGui::TextDisabled("This cannot be undone.");
            ImGui::Separator();
            if (ImGui::Button("Delete", ImVec2(120, 0))) {
                std::error_code ec;
                std::filesystem::remove_all(m_pendingDeletePath, ec);
                if (ec)
                    VE_CORE_WARN_PRINT("Delete failed: %s", m_pendingDeletePath.c_str());
                else
                    VE_CORE_SUCCESS_PRINT("Deleted: %s", m_pendingDeletePath.c_str());
                m_showDeleteConfirm = false;
                ImGui::CloseCurrentPopup();
                refreshFiles();
            }
            ImGui::SameLine();
            if (ImGui::Button("Cancel", ImVec2(120, 0))) {
                m_showDeleteConfirm = false;
                ImGui::CloseCurrentPopup();
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
