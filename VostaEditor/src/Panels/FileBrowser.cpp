#include "FileBrowser.h"
#include "AssetSaveRegistry.h"
#include "Renderer/Texture.h"
#include "Renderer/StaticMesh.h"
#include "Asset/ImportManager.h"
#include "Platform/OpenGL/OpenGLTexture.h"
#include "Asset/Utils.h"
#include "Core/Log.h"
#include "Core/ResourceManager.h"
#include "Renderer/Material.h"
#include "Renderer/LayeredMaterial.h"
#include "Renderer/MaterialLayerAsset.h"
#include "Renderer/ThumbnailRenderer.h"
#include "Noise/NoiseSettings.h"
#include "Noise/NoiseNodes.h"
#include "Scene/Archive.h"
#include <imgui.h>
#include <windows.h>
#include <commdlg.h>
#include <shlobj.h>
#include <cstdio>
#include <fstream>
#include <algorithm>
#include <cctype>

namespace ve {

    // Defined below; used by refreshFiles() and findBaked().
    static std::string normalizedStem(const std::string& path);

    // Win32 shell dialogs, kept here as the editor's single home for them so no
    // other translation unit has to pull in <windows.h>. "" when cancelled.

    std::string openImportFileDialog() {
        OPENFILENAMEA ofn = {};
        char szFile[1024] = {};
        ofn.lStructSize = sizeof(ofn);
        ofn.hwndOwner = GetActiveWindow();
        ofn.lpstrFile = szFile;
        ofn.nMaxFile = sizeof(szFile);
        ofn.lpstrFilter = "All\0*.*\0Textures\0*.png;*.jpg;*.hdr\0Models\0*.obj;*.fbx\0";
        ofn.nFilterIndex = 1;
        ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;
        if (GetOpenFileNameA(&ofn))
            return ofn.lpstrFile;
        return {};
    }

    namespace {
        int CALLBACK browseFolderCallback(HWND hwnd, UINT msg, LPARAM, LPARAM data) {
            if (msg == BFFM_INITIALIZED && data)
                SendMessageA(hwnd, BFFM_SETSELECTIONA, TRUE, data);
            return 0;
        }
    }

    std::string pickFolderDialog(const std::string& initial) {
        char display[MAX_PATH] = {};
        BROWSEINFOA bi = {};
        bi.hwndOwner      = GetActiveWindow();
        bi.pszDisplayName = display;
        bi.lpszTitle      = "Select a folder";
        bi.ulFlags        = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;
        bi.lpfn           = browseFolderCallback;
        bi.lParam         = reinterpret_cast<LPARAM>(initial.c_str());

        LPITEMIDLIST idl = SHBrowseForFolderA(&bi);
        if (!idl)
            return {};
        char path[MAX_PATH] = {};
        const bool ok = SHGetPathFromIDListA(idl, path) != FALSE;
        CoTaskMemFree(idl);
        return ok ? std::string(path) : std::string();
    }

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

    void FileBrowser::setOnFileSelect(FileSelectCallback callback) {
        m_onFileSelect = std::move(callback);
    }

    void FileBrowser::setOnSetDefaultScene(SetDefaultSceneCallback callback) {
        m_onSetDefaultScene = std::move(callback);
    }

    void FileBrowser::setDefaultScenePath(const std::string& path) {
        m_defaultScenePath = path;
    }

    void FileBrowser::requestRefresh() {
        m_refreshRequested = true;
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

        std::vector<FileEntry> entries;
        for (const auto& entry : std::filesystem::directory_iterator(m_currentPath)) {
            // Editor session state that lives beside a scene file (the camera
            // sidecar). Not an asset, so keep it out of the browsing surface.
            const std::string fileName = entry.path().filename().string();
            const std::string cameraSuffix = ".camera.json";
            if (fileName.size() > cameraSuffix.size()
                && fileName.compare(fileName.size() - cameraSuffix.size(),
                                    cameraSuffix.size(), cameraSuffix) == 0)
                continue;

            FileEntry e;
            e.name = entry.path().filename().string();
            e.path = entry.path().string();
            e.isDirectory = entry.is_directory();
            entries.push_back(e);
        }

        // Sort by asset kind (extension), folders ahead of files, then by name —
        // ".." stays at the front because it is pushed before this block.
        if (m_sortByType) {
            auto lowerExt = [](const FileEntry& f) {
                std::string ext = ve::utils::getExtension(f.path);
                std::transform(ext.begin(), ext.end(), ext.begin(),
                    [](unsigned char c) { return (char)std::tolower(c); });
                return ext;
            };
            std::sort(entries.begin(), entries.end(),
                [&](const FileEntry& a, const FileEntry& b) {
                    if (a.isDirectory != b.isDirectory) return a.isDirectory;
                    const std::string ea = lowerExt(a), eb = lowerExt(b);
                    if (ea != eb) return ea < eb;
                    return a.name < b.name;
                });
        }
        m_files.insert(m_files.end(), entries.begin(), entries.end());

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

    void FileBrowser::beginRename(const std::string& path, const std::string& initialName) {
        m_renamingPath = path;
        std::snprintf(m_renameBuffer, sizeof(m_renameBuffer), "%s", initialName.c_str());
        m_renameFocus = true;   // focus + select-all on the next frame
        m_selectedPath = path;
        m_contextPath.clear();
    }

    void FileBrowser::commitRename() {
        if (m_renamingPath.empty())
            return;

        const std::filesystem::path oldPath(m_renamingPath);
        const std::string oldName = oldPath.filename().string();
        std::string newName(m_renameBuffer);

        // Keep the original extension when the typed name omits one, so a rename
        // of "foo.veasset" to "bar" yields "bar.veasset".
        if (oldPath.has_extension() && newName.find('.') == std::string::npos)
            newName += oldPath.extension().string();

        m_renamingPath.clear();

        if (newName.empty() || newName == oldName)
            return;

        std::error_code ec;
        const std::filesystem::path newPath = oldPath.parent_path() / newName;
        if (std::filesystem::exists(newPath, ec)) {
            VE_CORE_WARN_PRINT("Rename: '%s' already exists", newPath.string().c_str());
            return;
        }

        std::filesystem::rename(oldPath, newPath, ec);
        if (ec) {
            VE_CORE_WARN_PRINT("Rename failed: %s", ec.message().c_str());
            return;
        }

        // Carry the loaded resources across the rename so handles in open
        // documents keep resolving instead of going stale.
        ResourceManager::renamePrefixAll(oldPath.string(), newPath.string());

        m_selectedPath = newPath.string();
        refreshFiles();
    }

    void FileBrowser::loadIcons() {
        m_folderIcon = std::shared_ptr<Texture2D>(Texture2D::create("VostaEditor/resources/icons/folder.png"));
        m_shaderIcon = std::shared_ptr<Texture2D>(Texture2D::create("VostaEditor/resources/icons/shader.png"));
        m_sourceIcon = std::shared_ptr<Texture2D>(Texture2D::create("VostaEditor/resources/icons/source_code.png"));
        m_materialIcon = std::shared_ptr<Texture2D>(Texture2D::create("VostaEditor/resources/icons/material.png"));
        m_staticMeshIcon = std::shared_ptr<Texture2D>(Texture2D::create("VostaEditor/resources/icons/static_mesh.png"));
        m_terrainIcon = std::shared_ptr<Texture2D>(Texture2D::create("VostaEditor/resources/icons/terrain.png"));
        m_noiseIcon = std::shared_ptr<Texture2D>(Texture2D::create("VostaEditor/resources/icons/noise_resource_2.png"));
        m_terrainDataIcon = std::shared_ptr<Texture2D>(Texture2D::create("VostaEditor/resources/icons/terrain_data.png"));
        m_notImportedIcon = std::shared_ptr<Texture2D>(Texture2D::create("VostaEditor/resources/icons/not_imported_file.png"));
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
        const ImU32 kNoise   = IM_COL32(90, 200, 170, 255);

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
            if (type == "noise")      return kNoise;
        }
        return kDefault;
    }

    // Name as shown in the browser: the baked-asset extensions are noise, so
    // they are hidden everywhere the name is displayed. commitRename() puts the
    // extension back when the typed name omits one.
    static std::string displayName(const std::string& name) {
        for (const std::string ext : { std::string(".veasset"), std::string(".veworld") }) {
            if (name.size() > ext.size() &&
                name.compare(name.size() - ext.size(), ext.size(), ext) == 0)
                return name.substr(0, name.size() - ext.size());
        }
        return name;
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
            if (assetType == "material" || assetType == "layered_material") {
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
            if (assetType == "material_layer") {
                // Preview the layer's albedo map: a layer has no baked sphere
                // thumbnail of its own, and its identity is its albedo.
                AssetHandle handle = ResourceManager::find<MaterialLayerAsset>(path);
                if (!handle.isValid()) {
                    handle = ResourceManager::store<MaterialLayerAsset>(path);
                }
                if (auto layer = ResourceManager::get<MaterialLayerAsset>(handle)) {
                    const std::string albedo = ResourceManager::getPath<Texture2D>(layer->layer.albedoMap);
                    if (!albedo.empty()) {
                        if (auto tex = texturePreviewForAsset(albedo))
                            return tex;
                    }
                }
                return m_materialIcon;
            }
            if (assetType == "noise") {
                return m_noiseIcon;
            }
            if (assetType == "terrain_data") {
                return m_terrainDataIcon;
            }
            // Unknown veasset type — fall through.
        }

        // Anything that is not a baked engine asset still has to be imported.
        return m_notImportedIcon;
    }

    void FileBrowser::render() {
        if (m_refreshRequested) {
            m_refreshRequested = false;
            refreshFiles();
        }

        ImGui::Begin("File Browser");

        if (ImGui::Button("Import")) {
            std::string filePath = openImportFileDialog();
            if (!filePath.empty()) {
                // Import into the directory the browser is currently showing.
                ImportManager::import(filePath, m_currentPath);
                refreshFiles();
            }
        }

        // Save every asset with unsaved edits. The count on the button is the
        // registry's dirty total, so it reflects editors that are not even open.
        ImGui::SameLine();
        auto& saves = AssetSaveRegistry::get();
        const std::size_t dirtyCount = saves.dirtyCount();
        const std::string saveLabel = dirtyCount > 0
            ? "Save (" + std::to_string(dirtyCount) + ")"
            : std::string("Save");
        if (dirtyCount > 0) {
            ImGui::PushStyleColor(ImGuiCol_Button, IM_COL32(196, 132, 36, 255));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, IM_COL32(216, 152, 56, 255));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, IM_COL32(176, 112, 26, 255));
        }
        if (ImGui::Button(saveLabel.c_str()))
            saves.saveAll();
        if (dirtyCount > 0)
            ImGui::PopStyleColor(3);

        // Organize the listing by asset kind (extension), folders first. Captured
        // before the click so the push/pop stay balanced when the state flips.
        ImGui::SameLine();
        const bool sortActive = m_sortByType;
        if (sortActive) {
            ImGui::PushStyleColor(ImGuiCol_Button, IM_COL32(64, 150, 250, 255));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, IM_COL32(92, 170, 255, 255));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, IM_COL32(44, 130, 235, 255));
        }
        if (ImGui::Button("Sort")) {
            m_sortByType = !m_sortByType;
            refreshFiles();
        }
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Sort by asset type (extension), folders first");
        if (sortActive)
            ImGui::PopStyleColor(3);

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

        // Lay the grid out by hand rather than with the legacy Columns() API: its
        // per-column clip rect is offset from the content area and clips the right
        // edge off every card. Dividing the available width exactly tiles the row.
        const float availW = ImGui::GetContentRegionAvail().x;
        int columns = (int)(availW / cellW);
        if (columns < 1) columns = 1;
        const float colW = availW / (float)columns;
        const ImVec2 gridMin = ImGui::GetCursorScreenPos();

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
        bool requestCommitRename = false;

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

        for (int idx = 0; idx < (int)m_files.size(); ++idx) {
            const auto& file = m_files[idx];
            auto icon = getIconForFile(file.path, file.isDirectory);

            ImGui::PushID(file.path.c_str());

            // Each card owns exactly one grid cell: place the hit box at the cell's
            // top-left, so the card fills the cell edge to edge.
            const int col = idx % columns;
            const int row = idx / columns;
            const ImVec2 cellMin(gridMin.x + col * colW, gridMin.y + row * cellH);
            const ImVec2 cellMax(cellMin.x + colW, cellMin.y + cellH);

            ImGui::SetCursorScreenPos(cellMin);
            ImGui::InvisibleButton("##cell", ImVec2(colW, cellH));
            const bool hovered = ImGui::IsItemHovered();
            const bool selected = (m_selectedPath == file.path);
            const bool renaming = (file.path == m_renamingPath);

            // While renaming, the cell is an input box: swallow navigation and
            // selection clicks so they don't fight the text field.
            if (!renaming) {
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

            // Thumbnail: fit into the tile box preserving aspect, centred on the
            // card (which spans the full column, not the nominal cellW).
            const ImVec2 imgAreaMin(cellMin.x + (colW - tile) * 0.5f, cellMin.y + pad);
            const ImVec2 imgAreaMax(imgAreaMin.x + tile, imgAreaMin.y + tile);

            // A rendered material ball leaves transparent margins inside its
            // square thumbnail, so at 1:1 it floats in empty space. Zoom it a
            // little: the overflow is alpha-0, so nothing clips visibly.
            float thumbZoom = 1.0f;
            if (!file.isDirectory) {
                auto typeIt = m_assetTypeCache.find(file.path);
                if (typeIt != m_assetTypeCache.end() &&
                    (typeIt->second == "material" || typeIt->second == "layered_material"))
                    thumbZoom = 1.3f;
            }

            if (icon && icon->getRendererID()) {
                const float tw = (float)icon->getWidth();
                const float th = (float)icon->getHeight();
                float scale = 1.0f;
                if (tw > 0.0f && th > 0.0f) {
                    const float sx = tile / tw, sy = tile / th;
                    scale = (sx < sy ? sx : sy) * thumbZoom;
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

            // Rule between the thumbnail and the name, tinted by asset kind. It
            // runs the full card width so it reads as a divider rather than a
            // floating dash under the icon.
            const float sepY = cellMin.y + sepYOffset;
            dl->AddLine(ImVec2(cellMin.x, sepY), ImVec2(cellMax.x, sepY), accent, 3.0f);

            const float textTop = sepY + pad;
            if (renaming) {
                // In-place input box replacing the name; the whole name is
                // selected on the first frame.
                ImGui::SetCursorScreenPos(ImVec2(cellMin.x + pad, textTop));
                ImGui::SetNextItemWidth(colW - pad * 2.0f);
                if (m_renameFocus) {
                    ImGui::SetKeyboardFocusHere();
                    m_renameFocus = false;
                }
                const bool entered = ImGui::InputText(
                    "##rename", m_renameBuffer, sizeof(m_renameBuffer),
                    ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AutoSelectAll);
                // Enter commits; clicking away deactivates (Escape reverts the
                // buffer first, so that path just no-ops into the same commit).
                if (entered || ImGui::IsItemDeactivated())
                    requestCommitRename = true;
                // Snap back onto the cell's own footprint. The Dummy matters:
                // it is what clears ImGui's "cursor was moved" flag, otherwise
                // the boundary check in End() trips once the window closes.
                ImGui::SetCursorScreenPos(ImVec2(cellMin.x, cellMax.y));
                ImGui::Dummy(ImVec2(0.0f, 0.0f));
            }
            else {
                std::string line1, line2;
                wrapTwoLines(displayName(file.name), colW - pad * 2.0f, line1, line2);

                const ImU32 textCol = IM_COL32(225, 225, 225, 255);
                const float w1 = ImGui::CalcTextSize(line1.c_str()).x;
                dl->AddText(ImVec2(cellMin.x + (colW - w1) * 0.5f, textTop), textCol, line1.c_str());
                if (!line2.empty()) {
                    const float w2 = ImGui::CalcTextSize(line2.c_str()).x;
                    dl->AddText(ImVec2(cellMin.x + (colW - w2) * 0.5f, textTop + lineH + spacingY),
                                textCol, line2.c_str());
                }
            }

            if (selected)
                dl->AddRect(cellMin, cellMax, ImGui::GetColorU32(ImGuiCol_ButtonActive),
                            rounding, 0, 2.0f);

            ImGui::PopID();
        }

        // Reserve the whole grid's height so the window scrolls to the last row.
        const int rows = ((int)m_files.size() + columns - 1) / columns;
        ImGui::SetCursorScreenPos(ImVec2(gridMin.x, gridMin.y + rows * cellH));
        ImGui::Dummy(ImVec2(0.0f, 0.0f));

        if (!openFile.empty() && m_onFileSelect)
            m_onFileSelect(openFile);
        if (!navigateTo.empty()) {
            m_currentPath = navigateTo;
            refreshFiles();
        }
        if (requestCommitRename)
            commitRename();

        // --- Item context menu ---
        if (requestItemPopup) {
            ImGui::OpenPopup("ItemContextMenu");
        }
        if (ImGui::BeginPopup("ItemContextMenu")) {
            // Full name, since the cell label truncates long ones.
            ImGui::TextDisabled("%s", displayName(m_contextName).c_str());
            ImGui::Separator();

            // Actions that only apply to one asset kind. A .veworld can be the
            // project's default scene; other kinds get their own entries here.
            if (ve::utils::getExtension(m_contextPath) == ".veworld") {
                std::error_code ec;
                const bool isDefault = !m_defaultScenePath.empty()
                    && std::filesystem::equivalent(m_contextPath, m_defaultScenePath, ec) && !ec;
                if (ImGui::MenuItem("Set as Default Scene", nullptr, isDefault) && !isDefault)
                    m_onSetDefaultScene(m_contextPath);
                ImGui::Separator();
            }

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
            // The ".." entry is a navigation shortcut, not a real file.
            if (ImGui::MenuItem("Rename", nullptr, false, m_contextName != ".."))
                beginRename(m_contextPath, displayName(m_contextName));
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
                // Drop straight into rename so the placeholder name can be typed over.
                beginRename(dir.string(), dir.filename().string());
            }
            else if (ImGui::MenuItem("Material")) {
                std::error_code ec;
                std::filesystem::path mat(m_currentPath + "/New Material.veasset");
                for (int i = 1; std::filesystem::exists(mat, ec); ++i)
                    mat = std::filesystem::path(m_currentPath + "/New Material " + std::to_string(i) + ".veasset");
                ResourceManager::store<Material>(mat.string());
                refreshFiles();
                beginRename(mat.string(), displayName(mat.filename().string()));
            }
            else if (ImGui::MenuItem("Layered Material")) {
                std::error_code ec;
                std::filesystem::path mat(m_currentPath + "/New Layered Material.veasset");
                for (int i = 1; std::filesystem::exists(mat, ec); ++i)
                    mat = std::filesystem::path(m_currentPath + "/New Layered Material " + std::to_string(i) + ".veasset");
                LayeredMaterial::writeNewAsset(mat.string());
                ResourceManager::store<Material>(mat.string());
                refreshFiles();
                beginRename(mat.string(), displayName(mat.filename().string()));
            }
            else if (ImGui::MenuItem("Material Layer")) {
                std::error_code ec;
                std::filesystem::path ml(m_currentPath + "/New Material Layer.veasset");
                for (int i = 1; std::filesystem::exists(ml, ec); ++i)
                    ml = std::filesystem::path(m_currentPath + "/New Material Layer " + std::to_string(i) + ".veasset");
                MaterialLayerAsset::writeNewAsset(ml.string());
                ResourceManager::store<MaterialLayerAsset>(ml.string());
                refreshFiles();
                beginRename(ml.string(), displayName(ml.filename().string()));
            }
            else if (ImGui::MenuItem("Noise")) {
                std::error_code ec;
                std::filesystem::path nz(m_currentPath + "/New Noise.veasset");
                for (int i = 1; std::filesystem::exists(nz, ec); ++i)
                    nz = std::filesystem::path(m_currentPath + "/New Noise " + std::to_string(i) + ".veasset");

                // Same on-disk contract as NoisePanel::saveToFile: the "noise" token,
                // the format version, then a default Noise Unit -> Output graph.
                TextArchive ar(nz.string(), ArchiveMode::write);
                if (ar.isGood()) {
                    NoiseGraph graph;
                    auto unit = CreateRef<NoiseUnitNode>();
                    auto out  = CreateRef<NoiseOutputNode>();
                    graph.addNode(unit, glm::vec2(-280.0f, 0.0f));
                    graph.addNode(out,  glm::vec2(60.0f, 0.0f));
                    graph.addLink(unit->m_outputPins[0].id, out->m_inputPins[0].id);

                    ar << std::string("noise");
                    ar << (int32_t)2;
                    serializeNoiseGraph(graph, ar);
                }
                else {
                    VE_CORE_WARN_PRINT("New Noise failed: %s", nz.string().c_str());
                }
                refreshFiles();
                beginRename(nz.string(), displayName(nz.filename().string()));
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
                else {
                    // A scene drags its camera sidecar along, or the orphan would
                    // outlive the scene it describes.
                    std::error_code sidecarEc;
                    std::filesystem::remove(m_pendingDeletePath + ".camera.json", sidecarEc);
                    VE_CORE_SUCCESS_PRINT("Deleted: %s", m_pendingDeletePath.c_str());
                }
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

        ImGui::End();
    }

}
