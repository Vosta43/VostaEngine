#include "Panels/EditorDialogs.h"

#include "EditorPaths.h"
#include "Panels/FileBrowser.h"

#include "Core/AssetConfig.h"
#include "Platform/Process.h"

#include "imgui.h"

#include <algorithm>
#include <cstdio>
#include <filesystem>

namespace ve {

namespace {

std::string formatFileSize(uintmax_t bytes) {
    char buf[32];
    if (bytes >= 1024ull * 1024ull)
        std::snprintf(buf, sizeof(buf), "%.1f MB", static_cast<double>(bytes) / (1024.0 * 1024.0));
    else if (bytes >= 1024ull)
        std::snprintf(buf, sizeof(buf), "%.1f KB", static_cast<double>(bytes) / 1024.0);
    else
        std::snprintf(buf, sizeof(buf), "%llu B", static_cast<unsigned long long>(bytes));
    return buf;
}

}

void EditorDialogs::setContext(const std::string& currentSceneName, const ProjectInfo& project) {
    m_currentSceneName = currentSceneName;
    m_projectName = project.name;
    m_projectVersion = project.version;
}

void EditorDialogs::openSave() {
    if (!m_currentSceneName.empty())
        std::snprintf(m_saveFileNameBuffer, sizeof(m_saveFileNameBuffer), "%s", m_currentSceneName.c_str());
    m_showSavePopup = true;
}

void EditorDialogs::openLoad() {
    if (!m_currentSceneName.empty())
        std::snprintf(m_loadFileNameBuffer, sizeof(m_loadFileNameBuffer), "%s", m_currentSceneName.c_str());
    refreshSceneFileList();
    m_showLoadPopup = true;
}

void EditorDialogs::openNewProject() {
    if (m_newProjectLocationBuffer[0] == '\0')
        std::snprintf(m_newProjectLocationBuffer, sizeof(m_newProjectLocationBuffer),
                      "%s", Project::projectsDir().c_str());
    m_showNewProjectPopup = true;
}

void EditorDialogs::openOpenProject() {
    refreshProjectList();
    m_showOpenProjectPopup = true;
}

void EditorDialogs::openPackage() {
    if (m_packageOutputBuffer[0] == '\0')
        std::snprintf(m_packageOutputBuffer, sizeof(m_packageOutputBuffer),
                      "%s", (getAssetRoot() / "Build").string().c_str());
    std::snprintf(m_packageVersionBuffer, sizeof(m_packageVersionBuffer),
                  "%s", m_projectVersion.c_str());
    m_packageError.clear();
    m_packageState = Packager::snapshot();
    m_showPackagePopup = true;
}

void EditorDialogs::refreshSceneFileList() {
    m_sceneFiles.clear();

    const std::filesystem::path dir = scenesDir();
    std::error_code ec;
    if (!std::filesystem::is_directory(dir, ec))
        return;

    for (const auto& entry : std::filesystem::directory_iterator(dir, ec)) {
        if (!entry.is_regular_file(ec))
            continue;
        if (entry.path().extension() != ".veworld")
            continue;

        SceneFile file;
        file.name = entry.path().filename().string();
        file.size = entry.file_size(ec);
        m_sceneFiles.push_back(file);
    }

    std::sort(m_sceneFiles.begin(), m_sceneFiles.end(),
        [](const SceneFile& a, const SceneFile& b) { return a.name < b.name; });
}

void EditorDialogs::refreshProjectList() {
    m_projectDirs.clear();

    std::error_code ec;
    const std::filesystem::path root = Project::projectsDir();
    if (std::filesystem::is_directory(root, ec)) {
        for (const auto& entry : std::filesystem::directory_iterator(root, ec)) {
            if (entry.is_directory(ec))
                m_projectDirs.push_back(entry.path().string());
        }
    }

    // The built-in SandBox project lives at the engine root, not under
    // Projects/, but is still openable.
    const std::filesystem::path sandbox = getAssetRoot() / "SandBox";
    if (std::filesystem::is_directory(sandbox, ec))
        m_projectDirs.push_back(sandbox.string());

    std::sort(m_projectDirs.begin(), m_projectDirs.end());
}

void EditorDialogs::onGuiRender() {
    // Save popup
    if (m_showSavePopup) {
        ImGui::OpenPopup("Save Scene");
        ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
        if (ImGui::BeginPopupModal("Save Scene", &m_showSavePopup)) {
            ImGui::InputText("Filename", m_saveFileNameBuffer, sizeof(m_saveFileNameBuffer));

            if (ImGui::Button("Save")) {
                if (m_callbacks.onSaveScene)
                    m_callbacks.onSaveScene(m_saveFileNameBuffer);
                m_showSavePopup = false;
            }

            ImGui::SameLine();

            if (ImGui::Button("Cancel"))
                m_showSavePopup = false;

            ImGui::EndPopup();
        }
    }

    // Load popup
    if (m_showLoadPopup) {
        ImGui::OpenPopup("Load Scene");
        ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
        if (ImGui::BeginPopupModal("Load Scene", &m_showLoadPopup)) {
            ImGui::TextDisabled("%s", toProjectAbsolute("content/scenes").c_str());
            ImGui::Separator();

            if (m_sceneFiles.empty()) {
                ImGui::TextDisabled("No .veworld files found");
            }
            else {
                // Size the list to its contents so no row is clipped away, but
                // cap it so a large folder still scrolls instead of filling the screen.
                const float listHeight = std::min(
                    ImGui::GetFrameHeightWithSpacing() * static_cast<float>(m_sceneFiles.size())
                        + ImGui::GetStyle().WindowPadding.y * 2.0f,
                    ImGui::GetMainViewport()->WorkSize.y * 0.6f);

                ImGui::BeginChild("##sceneList", ImVec2(460.0f, listHeight), true);
                ImGui::Columns(2, "##sceneColumns", false);
                ImGui::SetColumnWidth(0, 330.0f);

                for (const auto& file : m_sceneFiles) {
                    const bool selected = (m_loadFileName == file.name);

                    // SpanAllColumns so the whole row is clickable, not just the name.
                    if (ImGui::Selectable(file.name.c_str(), selected, ImGuiSelectableFlags_SpanAllColumns)) {
                        m_loadFileName = file.name;
                        std::snprintf(m_loadFileNameBuffer, sizeof(m_loadFileNameBuffer), "%s", file.name.c_str());
                    }

                    if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
                        m_loadFileName = file.name;
                        if (m_callbacks.onLoadScene)
                            m_callbacks.onLoadScene(file.name);
                        m_showLoadPopup = false;
                    }

                    ImGui::NextColumn();
                    ImGui::TextDisabled("%s", formatFileSize(file.size).c_str());
                    ImGui::NextColumn();
                }

                ImGui::Columns(1);
                ImGui::EndChild();
            }

            ImGui::InputText("Filename", m_loadFileNameBuffer, sizeof(m_loadFileNameBuffer));

            if (ImGui::Button("Load")) {
                m_loadFileName = m_loadFileNameBuffer;
                if (m_callbacks.onLoadScene)
                    m_callbacks.onLoadScene(m_loadFileNameBuffer);
                m_showLoadPopup = false;
            }

            ImGui::SameLine();

            if (ImGui::Button("Cancel"))
                m_showLoadPopup = false;

            ImGui::EndPopup();
        }
    }

    // New Project popup
    if (m_showNewProjectPopup) {
        ImGui::OpenPopup("New Project");
        ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
        if (ImGui::BeginPopupModal("New Project", &m_showNewProjectPopup)) {
            ImGui::InputText("Name", m_newProjectNameBuffer, sizeof(m_newProjectNameBuffer));
            ImGui::SetNextItemWidth(360.0f);
            ImGui::InputText("Location", m_newProjectLocationBuffer, sizeof(m_newProjectLocationBuffer));
            ImGui::SameLine();
            if (ImGui::Button("Browse...")) {
                const std::string picked = pickFolderDialog(m_newProjectLocationBuffer);
                if (!picked.empty())
                    std::snprintf(m_newProjectLocationBuffer, sizeof(m_newProjectLocationBuffer),
                                  "%s", picked.c_str());
            }

            ImGui::Checkbox("C++ gameplay module", &m_newProjectWithModule);
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("Generates src/Game.cpp and a standalone .slnx that builds Binaries/game.dll");

            if (ImGui::Button("Create")) {
                ProjectInfo info;
                const std::string parent = m_newProjectLocationBuffer[0]
                    ? m_newProjectLocationBuffer : Project::projectsDir();
                const std::string dir = (std::filesystem::path(parent)
                                         / m_newProjectNameBuffer).string();
                if (Project::create(parent, m_newProjectNameBuffer, m_newProjectWithModule, info)) {
                    if (m_callbacks.onApplyProject)
                        m_callbacks.onApplyProject(dir);
                    m_showNewProjectPopup = false;
                }
            }

            ImGui::SameLine();

            if (ImGui::Button("Cancel"))
                m_showNewProjectPopup = false;

            ImGui::EndPopup();
        }
    }

    // Open Project popup
    if (m_showOpenProjectPopup) {
        ImGui::OpenPopup("Open Project");
        ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
        if (ImGui::BeginPopupModal("Open Project", &m_showOpenProjectPopup)) {
            ImGui::TextDisabled("%s", Project::projectsDir().c_str());
            ImGui::Separator();

            if (m_projectDirs.empty()) {
                ImGui::TextDisabled("No projects found");
            }
            else {
                const float listHeight = std::min(
                    ImGui::GetFrameHeightWithSpacing() * static_cast<float>(m_projectDirs.size())
                        + ImGui::GetStyle().WindowPadding.y * 2.0f,
                    ImGui::GetMainViewport()->WorkSize.y * 0.6f);

                ImGui::BeginChild("##projectList", ImVec2(360.0f, listHeight), true);
                for (const auto& dir : m_projectDirs) {
                    const std::string name = std::filesystem::path(dir).filename().string();
                    const bool selected = (m_openProjectDir == dir);

                    if (ImGui::Selectable(name.c_str(), selected))
                        m_openProjectDir = dir;

                    if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
                        m_openProjectDir = dir;
                        if (m_callbacks.onApplyProject)
                            m_callbacks.onApplyProject(dir);
                        m_showOpenProjectPopup = false;
                    }
                }
                ImGui::EndChild();
            }

            if (ImGui::Button("Open")) {
                if (!m_openProjectDir.empty()) {
                    if (m_callbacks.onApplyProject)
                        m_callbacks.onApplyProject(m_openProjectDir);
                    m_showOpenProjectPopup = false;
                }
            }

            ImGui::SameLine();

            if (ImGui::Button("Browse...")) {
                const std::string start = m_openProjectDir.empty()
                    ? Project::projectsDir() : m_openProjectDir;
                const std::string picked = pickFolderDialog(start);
                if (!picked.empty()) {
                    m_openProjectDir = picked;
                    if (m_callbacks.onApplyProject)
                        m_callbacks.onApplyProject(picked);
                    m_showOpenProjectPopup = false;
                }
            }

            ImGui::SameLine();

            if (ImGui::Button("Cancel"))
                m_showOpenProjectPopup = false;

            ImGui::EndPopup();
        }
    }

    // Package Project popup
    if (m_showPackagePopup) {
        m_packageState = Packager::snapshot();
        const bool running = m_packageState.running;
        const bool done = m_packageState.stage == PackageStage::Done;

        ImGui::OpenPopup("Package Project");
        ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
        ImGui::SetNextWindowSize(ImVec2(920.0f, 560.0f), ImGuiCond_Appearing);
        if (ImGui::BeginPopupModal("Package Project", &m_showPackagePopup,
                                   ImGuiWindowFlags_NoSavedSettings)) {
            // The job writes the folder itself, so the dialog only mirrors the
            // name it will produce.
            const std::string version = m_packageVersionBuffer;
            const std::string folderName = version.empty() ? m_projectName : m_projectName + "-" + version;

            // Left: console transcript. Right: settings and progress.
            const float consoleWidth = ImGui::GetContentRegionAvail().x * 0.58f;

            ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.06f, 0.065f, 0.075f, 1.0f));
            ImGui::BeginChild("##package_console", ImVec2(consoleWidth, 0.0f), ImGuiChildFlags_Borders);
            const bool stickToBottom = ImGui::GetScrollY() >= ImGui::GetScrollMaxY() - 1.0f;
            if (m_packageState.log.empty()) {
                ImGui::TextDisabled("Waiting to start.");
            } else {
                for (const std::string& line : m_packageState.log) {
                    if (line.rfind("ERROR", 0) == 0)
                        ImGui::TextColored(ImVec4(1.00f, 0.45f, 0.45f, 1.0f), "%s", line.c_str());
                    else if (line.rfind("WARN", 0) == 0)
                        ImGui::TextColored(ImVec4(1.00f, 0.78f, 0.35f, 1.0f), "%s", line.c_str());
                    else if (line.rfind("DONE", 0) == 0)
                        ImGui::TextColored(ImVec4(0.45f, 0.90f, 0.50f, 1.0f), "%s", line.c_str());
                    else
                        ImGui::TextUnformatted(line.c_str());
                }
            }
            if (stickToBottom)
                ImGui::SetScrollHereY(1.0f);
            ImGui::EndChild();
            ImGui::PopStyleColor();

            ImGui::SameLine();
            ImGui::BeginChild("##package_options", ImVec2(0.0f, 0.0f), ImGuiChildFlags_None);

            ImGui::Text("Project: %s", m_projectName.c_str());
            ImGui::Spacing();

            ImGui::BeginDisabled(running);
            const float browseWidth = ImGui::GetFrameHeight() + ImGui::CalcTextSize("Browse...").x + 8.0f;
            ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - browseWidth);
            ImGui::InputTextWithHint("##package_output", "Output folder",
                                     m_packageOutputBuffer, sizeof(m_packageOutputBuffer));
            ImGui::SameLine();
            if (ImGui::Button("Browse...")) {
                const std::string picked = pickFolderDialog(m_packageOutputBuffer);
                if (!picked.empty())
                    std::snprintf(m_packageOutputBuffer, sizeof(m_packageOutputBuffer), "%s", picked.c_str());
            }

            ImGui::SetNextItemWidth(160.0f);
            ImGui::InputTextWithHint("##package_version", "Version (optional)",
                                     m_packageVersionBuffer, sizeof(m_packageVersionBuffer));
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("Appended to the output folder name; recorded in build.json");

            ImGui::TextDisabled("%s", (std::filesystem::path(m_packageOutputBuffer) / folderName).string().c_str());
            ImGui::Spacing();

            ImGui::Checkbox("Rebuild gameplay module", &m_packageRebuildModule);
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("On: run MSBuild on the project's .slnx first. "
                                  "Off: package the existing Binaries/game.dll");

            ImGui::BeginDisabled(!m_packageRebuildModule);
            ImGui::Checkbox("Release", &m_packageRelease);
            ImGui::EndDisabled();
            if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
                ImGui::SetTooltip(m_packageRebuildModule
                    ? "On: build the shipping Release module. Off: Debug (links the debug CRT)"
                    : "Only used when rebuilding the gameplay module");

            ImGui::Spacing();
            ImGui::TextUnformatted("Layout");
            ImGui::RadioButton("Loose files", &m_packageMode, 0);
            ImGui::SameLine();
            ImGui::RadioButton("PAK", &m_packageMode, 1);
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("One VEGPAK archive with zstd-compressed 64 KiB chunks. "
                                  "The player mounts it automatically at startup");
            ImGui::SameLine();
            ImGui::BeginDisabled(true);
            ImGui::RadioButton("PAK encrypted", &m_packageMode, 2);
            ImGui::EndDisabled();
            if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
                ImGui::SetTooltip("Not implemented yet: archives are written unencrypted");
            ImGui::EndDisabled();

            ImGui::Separator();

            // Stage list: the running stage lit, finished ones dimmed.
            static const char* kStages[] = {
                "Build gameplay module", "Collect content", "Assemble runtime", "Write metadata"
            };
            for (int i = 0; i < m_packageState.stepCount; ++i) {
                const bool current  = running && m_packageState.step == i + 1;
                const bool finished = done || m_packageState.step > i + 1;
                const ImVec4 color = current ? ImVec4(0.40f, 0.80f, 1.00f, 1.0f)
                                   : finished ? ImVec4(0.55f, 0.55f, 0.55f, 1.0f)
                                              : ImVec4(0.75f, 0.75f, 0.75f, 1.0f);
                ImGui::TextColored(color, "%s %s",
                                   finished ? "[x]" : (current ? "[>]" : "[ ]"), kStages[i]);
            }

            if (!m_packageState.message.empty())
                ImGui::TextWrapped("%s", m_packageState.message.c_str());
            if (m_packageState.stage == PackageStage::Failed)
                ImGui::TextWrapped("%s", m_packageState.detail.c_str());
            if (!m_packageError.empty())
                ImGui::TextWrapped("%s", m_packageError.c_str());

            ImGui::Separator();

            ImGui::BeginDisabled(running);
            if (ImGui::Button("Package")) {
                PackageOptions opts;
                opts.projectRoot   = getProjectRoot().string();
                opts.outputDir     = m_packageOutputBuffer;
                opts.rebuildModule = m_packageRebuildModule;
                opts.release       = m_packageRelease;
                opts.version     = m_packageVersionBuffer;
                opts.mode        = static_cast<PackMode>(m_packageMode);
                std::string error;
                if (!Packager::start(opts, error))
                    m_packageError = "Could not start: " + error;
                m_packageState = Packager::snapshot();
            }
            ImGui::EndDisabled();

            if (done) {
                ImGui::SameLine();
                const std::string& outDir = m_packageState.detail;
                if (ImGui::Button("Open Folder"))
                    runProcess({ "explorer", outDir });
                ImGui::SameLine();
                if (ImGui::Button("Launch"))
                    runProcess({ (std::filesystem::path(outDir) / (m_projectName + ".exe")).string() },
                               outDir, /*wait=*/false);
            }

            ImGui::SameLine();
            if (ImGui::Button("Close")) {
                m_packageError.clear();
                m_packageState = PackageState{};
                m_showPackagePopup = false;
            }

            ImGui::EndChild();
            ImGui::EndPopup();
        }
    }
}

}
