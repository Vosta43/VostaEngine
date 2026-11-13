#include "vepch.h"
#include "Packager.h"

#include "Asset/PakKey.h"
#include "Asset/PakReader.h"
#include "Build/PakWriter.h"
#include "Core/AssetConfig.h"
#include "Core/Json.h"
#include "Core/Log.h"
#include "Core/Project.h"
#include "Platform/Process.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <set>
#include <thread>

namespace ve {

namespace {

    namespace fs = std::filesystem;

    // Mirrors project() VERSION in CMakeLists.txt, recorded in build.json so a
    // shipped folder can be traced back to the engine that produced it.
    constexpr const char* kEngineVersion = "0.2.15";

    // zstd's default level: packaging stays interactive; shipping size is close
    // enough to level 19 for content that is mostly already-compressed images.
    constexpr int kPackCompressionLevel = 3;

    // ── Job state ───────────────────────────────────────────────────────────
    // One job at a time. The worker thread writes under the mutex; the UI reads
    // a copy through snapshot().
    std::mutex   g_mutex;
    PackageState g_state;

    // Caller holds g_mutex. The console is a progress transcript, so it is
    // capped instead of growing for the whole job.
    void appendLocked(const std::string& line) {
        g_state.log.push_back(line);
        if (g_state.log.size() > 300) g_state.log.erase(g_state.log.begin());
    }

    void logLine(const std::string& line) {
        std::lock_guard<std::mutex> lock(g_mutex);
        appendLocked(line);
    }

    void setStage(PackageStage stage, int step, const std::string& message) {
        std::lock_guard<std::mutex> lock(g_mutex);
        g_state.stage   = stage;
        g_state.step    = step;
        g_state.message = message;
        appendLocked("[" + std::to_string(step) + "/" + std::to_string(g_state.stepCount) + "] " + message);
    }

    void fail(const std::string& detail) {
        std::lock_guard<std::mutex> lock(g_mutex);
        g_state.stage   = PackageStage::Failed;
        g_state.detail  = detail;
        g_state.running = false;
        appendLocked("ERROR: " + detail);
    }

    // ── Helpers ─────────────────────────────────────────────────────────────

    fs::path sdkRoot() { return getAssetRoot() / "SDK"; }

    // The SDK is the source of truth for a redistributable payload. A partial
    // assembly looks plausible but produces a folder that cannot launch, so test
    // for the files, not just the folders -- the same guard Project::create uses.
    bool sdkPayloadReady(std::string& missing) {
        static const char* kRequired[] = {
            "bin/VostaPlayer.exe",
            "bin/VostaEngine.dll",
            "VostaEngine/resources",
        };
        std::error_code ec;
        for (const char* rel : kRequired) {
            if (!fs::exists(sdkRoot() / rel, ec)) {
                missing = rel;
                return false;
            }
        }
        return true;
    }

    // vswhere ships with every VS 2017+ installer at a fixed location, so it is
    // the one reliable way to find a toolchain without hard-coding an edition.
    // getenv is flagged unsafe by the MSVC CRT, and this project's /sdl promotes
    // that warning to an error; _dupenv_s is the sanctioned replacement (the same
    // helper EnginePaths.cpp uses).
    std::string envVar(const char* name) {
        char* value = nullptr;
        size_t length = 0;
        if (_dupenv_s(&value, &length, name) != 0 || value == nullptr) return {};
        std::string result(value);
        std::free(value);
        return result;
    }

    std::string findWithVswhere(const std::vector<std::string>& vswhereArgs) {
        std::string pf = envVar("ProgramFiles(x86)");
        if (pf.empty()) pf = envVar("ProgramFiles");
        if (pf.empty()) return {};

        const std::string vswhere =
            pf + "\\Microsoft Visual Studio\\Installer\\vswhere.exe";
        std::error_code ec;
        if (!fs::exists(vswhere, ec)) return {};

        // Each flag is its own argv element: vswhere parses argv directly, so a
        // single pre-joined string is read as one unknown parameter (exit 87).
        std::vector<std::string> argv{ vswhere };
        argv.insert(argv.end(), vswhereArgs.begin(), vswhereArgs.end());

        const ProcessResult r = runProcess(argv);
        if (!r.started || r.exitCode != 0) return {};

        // vswhere can list several matches (a VS instance may carry more than one
        // vc_redist build); the first is the one to use, so cut at the first line.
        std::string out = r.output;
        const size_t lineEnd = out.find_first_of("\r\n");
        if (lineEnd != std::string::npos) out.resize(lineEnd);
        while (!out.empty() && out.back() == ' ') out.pop_back();
        return out;
    }

    std::string findMsbuild() {
        return findWithVswhere({ "-latest", "-products", "*",
                                 "-requires", "Microsoft.Component.MSBuild",
                                 "-find", "MSBuild\\**\\Bin\\MSBuild.exe" });
    }

    std::string findVcRedist() {
        return findWithVswhere({ "-latest", "-find",
                                 "VC\\Redist\\MSVC\\**\\vc_redist.x64.exe" });
    }

    std::string tail(const std::string& text, size_t maxChars = 2000) {
        if (text.size() <= maxChars) return text;
        return "..." + text.substr(text.size() - maxChars);
    }

    std::string utcNow() {
        const std::time_t now = std::time(nullptr);
        std::tm tm{};
        gmtime_s(&tm, &now);
        char buf[32] = {};
        std::strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%SZ", &tm);
        return buf;
    }

    bool isSkipped(const std::string& rel, const std::vector<std::string>& skipRel) {
        for (const std::string& skip : skipRel) {
            if (rel == skip) return true;
            if (rel.size() > skip.size() && rel.compare(0, skip.size(), skip) == 0 &&
                rel[skip.size()] == '/')
                return true;
        }
        return false;
    }

    // Recursive copy. `skipRel` names files or folders (relative to `from`, '/'
    // separated) that a shipped build must not carry: the editor camera sidecar
    // is authoring session state, and the engine's mcp/ (tooling docs) and
    // temps/ (authoring art) have no reader at runtime.
    bool copyTreeFiltered(const fs::path& from, const fs::path& to,
                          const std::vector<std::string>& skipRel,
                          std::string& error) {
        std::error_code ec;
        fs::create_directories(to, ec);

        fs::recursive_directory_iterator it(
            from, fs::directory_options::skip_permission_denied, ec);
        const fs::recursive_directory_iterator end;
        for (; !ec && it != end; it.increment(ec)) {
            const fs::path rel = fs::relative(it->path(), from, ec);
            if (ec) { error = "path resolution failed under " + from.string(); return false; }

            const std::string relText = rel.generic_string();
            if (it->is_directory(ec)) {
                if (relText != "." && isSkipped(relText, skipRel))
                    it.disable_recursion_pending();
                continue;
            }
            if (isSkipped(relText, skipRel)) continue;

            const std::string name = it->path().filename().string();
            if (name.size() >= 12 && name.ends_with(".camera.json")) continue;

            const fs::path dst = to / rel;
            fs::create_directories(dst.parent_path(), ec);
            fs::copy_file(it->path(), dst, fs::copy_options::overwrite_existing, ec);
            if (ec) { error = "copy failed: " + it->path().string(); return false; }
        }
        if (ec) { error = "scan failed under " + from.string(); return false; }
        return true;
    }

    // ── Package manifest ────────────────────────────────────────────────────
    // Both layouts collect first and write second, so loose files and the PAK
    // carry exactly the same set (and package.exclude is matched once, against
    // the package-root-relative path).

    struct PendingFile {
        std::string pakPath;  // '/'-separated, package-root-relative
        fs::path    source;
    };

    bool collectTree(const fs::path& from, const std::string& prefix,
                     const std::vector<std::string>& skipPakRel,
                     std::vector<PendingFile>& out, std::string& error) {
        std::error_code ec;
        fs::recursive_directory_iterator it(
            from, fs::directory_options::skip_permission_denied, ec);
        const fs::recursive_directory_iterator end;
        for (; !ec && it != end; it.increment(ec)) {
            const fs::path rel = fs::relative(it->path(), from, ec);
            if (ec) { error = "path resolution failed under " + from.string(); return false; }

            const std::string relText = rel.generic_string();
            const std::string pakPath = prefix.empty() ? relText : prefix + "/" + relText;
            if (it->is_directory(ec)) {
                if (relText != "." && isSkipped(pakPath, skipPakRel))
                    it.disable_recursion_pending();
                continue;
            }
            if (isSkipped(pakPath, skipPakRel)) continue;

            const std::string name = it->path().filename().string();
            if (name.size() >= 12 && name.ends_with(".camera.json")) continue;

            out.push_back({ pakPath, it->path() });
        }
        if (ec) { error = "scan failed under " + from.string(); return false; }
        return true;
    }

    std::string formatBytes(uint64_t bytes) {
        char buf[32];
        if (bytes >= 1024ull * 1024ull)
            std::snprintf(buf, sizeof(buf), "%.1f MB", static_cast<double>(bytes) / (1024.0 * 1024.0));
        else if (bytes >= 1024ull)
            std::snprintf(buf, sizeof(buf), "%.1f KB", static_cast<double>(bytes) / 1024.0);
        else
            std::snprintf(buf, sizeof(buf), "%llu B", static_cast<unsigned long long>(bytes));
        return buf;
    }

    // ── Reference check ─────────────────────────────────────────────────────
    // A package fails at runtime, not at build time, when an asset points at a
    // file the job did not copy: the file is simply absent and the loader falls
    // back to a default. Walking the copied text assets catches the paths the
    // engine can see; module-owned data (paths baked into game code) cannot be
    // inferred and is what package.include exists for.

    constexpr size_t kMaxScannedBytes = 8u * 1024u * 1024u;

    bool readTextFile(const fs::path& path, std::string& out) {
        std::ifstream in(path, std::ios::binary);
        if (!in) return false;
        in.seekg(0, std::ios::end);
        const std::streamoff size = in.tellg();
        if (size <= 0 || static_cast<size_t>(size) > kMaxScannedBytes) return false;
        in.seekg(0, std::ios::beg);
        out.resize(static_cast<size_t>(size));
        in.read(out.data(), static_cast<std::streamsize>(out.size()));
        return static_cast<bool>(in);
    }

    bool isReferenceFile(const std::string& name) {
        return name.ends_with(".veworld") || name.ends_with(".veasset");
    }

    // A stored path always carries an asset extension; anything else quoted in a
    // scene (a component name, an enum value) is not a file reference.
    bool looksLikeAssetPath(const std::string& value) {
        static const char* kExtensions[] = {
            ".veasset", ".veworld", ".png", ".jpg", ".jpeg", ".tga", ".hdr", ".exr",
            ".obj", ".fbx", ".gltf", ".glb", ".mtl", ".ttf", ".otf",
            ".ogg", ".wav", ".mp3", ".flac"
        };
        for (const char* ext : kExtensions)
            if (value.ends_with(ext)) return true;
        return false;
    }

    // Project roots ship at the package root, and the engine tree ships with the
    // SDK payload, so both resolve in a package. Everything else does not.
    bool isCovered(const std::string& value, const std::vector<std::string>& roots) {
        if (value == "VostaEngine" || value.rfind("VostaEngine/", 0) == 0) return true;
        for (const std::string& root : roots) {
            if (value == root) return true;
            if (value.size() > root.size() && value.compare(0, root.size(), root) == 0 &&
                value[root.size()] == '/')
                return true;
        }
        return false;
    }

    void collectStrayReferences(const std::vector<PendingFile>& files,
                                const std::vector<std::string>& roots,
                                std::vector<std::string>& warnings) {
        std::set<std::string> seen;
        for (const PendingFile& pending : files) {
            if (!isReferenceFile(pending.source.filename().string())) continue;

            std::string text;
            if (!readTextFile(pending.source, text)) continue;
            const std::string& file = pending.pakPath;

            size_t pos = 0;
            while ((pos = text.find('"', pos)) != std::string::npos) {
                const size_t close = text.find('"', pos + 1);
                if (close == std::string::npos) break;

                std::string value = text.substr(pos + 1, close - pos - 1);
                pos = close + 1;

                if (value.size() < 4 || value.size() > 260) continue;
                if (!looksLikeAssetPath(value)) continue;
                std::replace(value.begin(), value.end(), '\\', '/');
                if (value.find('/') == std::string::npos) continue;  // bare key, not a path
                if (isCovered(value, roots)) continue;
                if (warnings.size() >= 25) return;

                if (seen.insert(file + '|' + value).second)
                    warnings.push_back(file + " -> " + value);
            }
        }
    }

    // ── The job ─────────────────────────────────────────────────────────────

    void runJob(PackageOptions opts) {
        std::error_code ec;

        ProjectInfo project;
        if (!Project::open(opts.projectRoot, project)) {
            fail("Not a project folder: " + opts.projectRoot);
            return;
        }

        std::string missing;
        if (!sdkPayloadReady(missing)) {
            fail("Engine SDK payload is incomplete ('" + missing + "' missing under " +
                 sdkRoot().string() + "). Build the VostaEngineSdk project, then retry.");
            return;
        }

        // The version is a folder suffix rather than a nested folder, so the
        // packaged exe stays at the project root -- exactly where the player
        // expects to find project.json and the content roots.
        std::string folderName = project.name;
        if (!opts.version.empty()) {
            if (opts.version.find_first_of("\\/:*?\"<>|") != std::string::npos) {
                fail("Version contains a character a folder name cannot: " + opts.version);
                return;
            }
            folderName += "-" + opts.version;
        }

        const fs::path outRoot = fs::path(opts.outputDir).lexically_normal() / folderName;
        if (fs::exists(outRoot, ec)) {
            fail("Output folder already exists: " + outRoot.string());
            return;
        }
        fs::create_directories(outRoot, ec);
        if (ec) { fail("Could not create " + outRoot.string()); return; }
        logLine("Output: " + outRoot.string());

        // ── 1. Gameplay module ──────────────────────────────────────────────
        const fs::path slnx =
            fs::path(opts.projectRoot).lexically_normal() / (project.name + ".slnx");
        const bool hasModule = fs::exists(slnx, ec);
        const fs::path module =
            fs::path(opts.projectRoot).lexically_normal() / "Binaries" / "game.dll";

        if (!opts.rebuildModule) {
            // Reuse what the user already built. A module project without an
            // output would otherwise package as an empty scene at runtime.
            if (hasModule && !fs::exists(module, ec)) {
                fail("Binaries/game.dll was not found. Build the project first, or turn on "
                     "\"Rebuild gameplay module\".");
                return;
            }
            setStage(PackageStage::BuildModule, 1,
                     hasModule ? "Reusing the built gameplay module"
                               : "No gameplay module, skipping the build");
        } else if (hasModule) {
            setStage(PackageStage::BuildModule, 1, "Building the gameplay module");
            const std::string msbuild = findMsbuild();
            if (msbuild.empty()) {
                fail("MSBuild was not found. Install Visual Studio with the C++ workload, "
                     "or turn off \"Rebuild gameplay module\" to package an existing Binaries/game.dll.");
                return;
            }
            const std::string config = opts.release ? "Release" : "Debug";
            const ProcessResult r = runProcess(
                { msbuild, slnx.string(), "-nologo", "-v:minimal", "-m",
                  "-p:Configuration=" + config, "-p:Platform=x64" },
                opts.projectRoot);
            if (!r.started) { fail("Could not launch MSBuild."); return; }
            if (r.exitCode != 0) { fail("MSBuild failed:\n" + tail(r.output)); return; }
        } else {
            // No module scaffolded: the player still loads and renders the scene.
            setStage(PackageStage::BuildModule, 1, "No gameplay module, skipping the build");
        }

        // ── 2. Project content ──────────────────────────────────────────────
        // package.include names the folders that ship (project.json). A project
        // that declares none ships content/ only -- the layout every project had
        // before the field existed. Both layouts collect the same manifest first;
        // only the write step differs.
        setStage(PackageStage::CollectContent, 2, "Collecting content");
        std::string err;
        const fs::path projectSource = fs::path(opts.projectRoot).lexically_normal();
        const std::vector<std::string> roots = Project::packageRoots(project);
        const bool declared = !project.packageInclude.empty();

        std::vector<PendingFile> manifest;
        for (const std::string& root : roots) {
            const fs::path src = projectSource / root;
            ec.clear();
            if (!fs::exists(src, ec)) {
                // The implicit default stays tolerant, as it was before the list
                // existed; a hand-written entry is a typo worth stopping for.
                if (!declared && root == "content") {
                    logLine("  no content/ folder, skipping");
                    continue;
                }
                fail("package.include entry '" + root + "' does not exist under " +
                     projectSource.string());
                return;
            }

            const size_t before = manifest.size();
            const bool directory = fs::is_directory(src, ec);
            if (directory) {
                if (!collectTree(src, root, project.packageExclude, manifest, err)) {
                    fail("Collecting '" + root + "' failed: " + err);
                    return;
                }
            } else {
                manifest.push_back({ root, src });
            }

            const std::string verb = opts.mode == PackMode::Loose ? "copy " : "pack ";
            logLine("  " + verb + root + (directory ? "/" : "") + " (" +
                    std::to_string(manifest.size() - before) + " files)");
        }

        // Overlapping package.include roots can name one file twice; an archive
        // cannot, and a loose copy would only do redundant work.
        {
            std::set<std::string> seen;
            std::vector<PendingFile> unique;
            unique.reserve(manifest.size());
            for (PendingFile& file : manifest) {
                if (seen.insert(file.pakPath).second)
                    unique.push_back(std::move(file));
                else
                    logLine("  skip duplicate " + file.pakPath);
            }
            manifest.swap(unique);
        }

        if (opts.mode != PackMode::Loose) {
            const fs::path archive = outRoot / (project.name + ".pak");
            pak::Writer writer;
            if (opts.mode == PackMode::PakEncrypted) {
                if (!writer.openEncrypted(archive.string(), pak::kBuiltinKeyId, &err)) {
                    fail("Could not create " + archive.filename().string() + ": " + err);
                    return;
                }
                logLine("  encrypted with content key #" +
                        std::to_string(pak::kBuiltinKeyId));
            } else if (!writer.open(archive.string(), &err)) {
                fail("Could not create " + archive.filename().string() + ": " + err);
                return;
            }
            for (const PendingFile& file : manifest) {
                if (!writer.addFile(file.pakPath, file.source.string(),
                                    kPackCompressionLevel, &err)) {
                    fail("PAK: " + err);
                    return;
                }
            }
            if (!writer.finish(&err)) {
                fail("PAK: " + err);
                return;
            }

            // A package that cannot be read back is worse than a failed build:
            // decode every entry through the same reader the runtime will use.
            pak::Reader reader;
            if (!reader.open(archive.string(), &err)) {
                fail("PAK verification failed to open the archive: " + err);
                return;
            }
            std::vector<std::string> names;
            reader.list(names);
            uint64_t rawBytes = 0;
            for (const std::string& name : names) {
                uint64_t size = 0;
                reader.fileSize(name, size);
                rawBytes += size;
                if (!reader.verifyFile(name, &err)) {
                    fail("PAK verification failed for '" + name + "': " + err);
                    return;
                }
            }
            ec.clear();
            const uintmax_t storedBytes = fs::file_size(archive, ec);
            logLine("  " + archive.filename().string() + ": " +
                    std::to_string(names.size()) + " files, " + formatBytes(rawBytes) +
                    " -> " + formatBytes(ec ? 0 : static_cast<uint64_t>(storedBytes)));
        } else {
            for (const PendingFile& file : manifest) {
                const fs::path dst = outRoot / file.pakPath;
                ec.clear();
                fs::create_directories(dst.parent_path(), ec);
                if (ec) { fail("Content copy failed: " + dst.string()); return; }
                fs::copy_file(file.source, dst, fs::copy_options::overwrite_existing, ec);
                if (ec) { fail("Content copy failed: " + file.source.string()); return; }
            }
        }

        // Report references the package cannot satisfy now, rather than at the
        // user's first launch.
        std::vector<std::string> warnings;
        collectStrayReferences(manifest, roots, warnings);
        {
            std::lock_guard<std::mutex> lock(g_mutex);
            g_state.warnings = warnings;
            for (const std::string& warning : warnings) appendLocked("WARN " + warning);
        }

        // ── 3. Runtime ──────────────────────────────────────────────────────
        setStage(PackageStage::AssembleRuntime, 3, "Assembling the runtime");
        const fs::path sdk = sdkRoot();

        ec.clear();
        fs::copy_file(sdk / "bin" / "VostaPlayer.exe", outRoot / (project.name + ".exe"),
                      fs::copy_options::overwrite_existing, ec);
        if (ec) { fail("Could not copy the player executable."); return; }

        ec.clear();
        fs::copy_file(sdk / "bin" / "VostaEngine.dll", outRoot / "VostaEngine.dll",
                      fs::copy_options::overwrite_existing, ec);
        if (ec) { fail("Could not copy VostaEngine.dll."); return; }

        // The player loads the module from <projectRoot>/Binaries/game.dll, and in
        // a package the project root is the output folder -- so the build output
        // travels with it instead of staying in the source project.
        ec.clear();
        if (fs::exists(module, ec)) {
            fs::create_directories(outRoot / "Binaries", ec);
            fs::copy_file(module, outRoot / "Binaries" / "game.dll",
                          fs::copy_options::overwrite_existing, ec);
            if (ec) { fail("Could not copy the gameplay module."); return; }
        }

        // shaders/, pipelines/, fonts/, models/ and textures/ are all load-time
        // required -- a saved scene references built-ins that resolve through
        // them -- so only the authoring-only folders are dropped.
        if (!copyTreeFiltered(sdk / "VostaEngine" / "resources",
                              outRoot / "VostaEngine" / "resources",
                              { "mcp", "temps" }, err)) {
            fail("Engine resource copy failed: " + err);
            return;
        }

        // Release links the CRT dynamically (/MD), so a machine without the VC++
        // runtime cannot start the build; ship the installer beside it.
        const std::string redist = findVcRedist();
        ec.clear();
        if (!redist.empty()) {
            fs::copy_file(redist, outRoot / "vc_redist.x64.exe",
                          fs::copy_options::overwrite_existing, ec);
        } else {
            VE_CORE_WARN_PRINT("%s", "Packager: vc_redist.x64.exe not found; the target "
                                     "machine must have the VC++ runtime installed separately");
        }

        // Licences travel with the binaries.
        for (const char* doc : { "LICENSE.txt", "THIRD_PARTY_NOTICES.md" }) {
            const fs::path src = getAssetRoot() / doc;
            ec.clear();
            if (fs::exists(src, ec))
                fs::copy_file(src, outRoot / doc, fs::copy_options::overwrite_existing, ec);
        }

        // ── 4. Metadata ─────────────────────────────────────────────────────
        setStage(PackageStage::WriteMeta, 4, "Writing metadata");
        {
            JsonWriter w;
            w.set("name", project.name);
            w.set("defaultScene", project.defaultScene);
            w.set("presentation", Project::presentationName(project.presentation));
            if (!w.writeToFile((outRoot / "project.json").string())) {
                fail("Could not write project.json.");
                return;
            }

            JsonWriter b;
            b.set("engineVersion", kEngineVersion);
            b.set("configuration", opts.release ? std::string("Release") : std::string("Debug"));
            b.set("builtUtc", utcNow());
            b.set("layout", opts.mode == PackMode::Loose ? std::string("loose")
                     : opts.mode == PackMode::PakEncrypted ? std::string("pak-encrypted")
                                                           : std::string("pak"));
            if (!opts.version.empty()) b.set("version", opts.version);
            b.writeToFile((outRoot / "build.json").string());
        }

        std::lock_guard<std::mutex> lock(g_mutex);
        g_state.stage   = PackageStage::Done;
        g_state.step    = g_state.stepCount;
        g_state.message = "Packaged: " + outRoot.string();
        g_state.detail  = outRoot.string();
        g_state.running = false;
        appendLocked("DONE " + outRoot.string());
    }

} // namespace

namespace Packager {

bool start(const PackageOptions& options, std::string& error) {
    if (options.projectRoot.empty() || options.outputDir.empty()) {
        error = "project root and output folder must both be set";
        return false;
    }

    {
        std::lock_guard<std::mutex> lock(g_mutex);
        if (g_state.running) {
            error = "a packaging job is already running";
            return false;
        }
        g_state = PackageState{};
        g_state.stage     = PackageStage::BuildModule;
        g_state.step      = 1;
        g_state.stepCount = 4;
        g_state.message   = "Starting";
        g_state.running   = true;
        appendLocked("Starting package job");
    }

    // Detached: the job reports through g_state under the mutex and owns no
    // session state, so a shutdown mid-package needs no unwinding (a joinable
    // thread left at static destruction would terminate the process).
    std::thread(runJob, options).detach();
    return true;
}

PackageState snapshot() {
    std::lock_guard<std::mutex> lock(g_mutex);
    return g_state;
}

} // namespace Packager

} // namespace ve
