#include "vepch.h"
#include "Packager.h"

#include "Core/AssetConfig.h"
#include "Core/Json.h"
#include "Core/Log.h"
#include "Core/Project.h"
#include "Platform/Process.h"

#include <cstdlib>
#include <ctime>
#include <filesystem>
#include <mutex>
#include <thread>

namespace ve {

namespace {

    namespace fs = std::filesystem;

    // Mirrors project() VERSION in CMakeLists.txt, recorded in build.json so a
    // shipped folder can be traced back to the engine that produced it.
    constexpr const char* kEngineVersion = "0.2.9";

    // ── Job state ───────────────────────────────────────────────────────────
    // One job at a time. The worker thread writes under the mutex; the UI reads
    // a copy through snapshot().
    std::mutex   g_mutex;
    PackageState g_state;

    void setStage(PackageStage stage, int step, const std::string& message) {
        std::lock_guard<std::mutex> lock(g_mutex);
        g_state.stage   = stage;
        g_state.step    = step;
        g_state.message = message;
    }

    void fail(const std::string& detail) {
        std::lock_guard<std::mutex> lock(g_mutex);
        g_state.stage   = PackageStage::Failed;
        g_state.detail  = detail;
        g_state.running = false;
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

    // Recursive copy that drops files a shipped build must not carry: the editor
    // camera sidecar is authoring session state, and the engine's mcp/ (tooling
    // docs) and temps/ (authoring art) have no reader at runtime.
    bool copyTreeFiltered(const fs::path& from, const fs::path& to,
                          const std::vector<std::string>& skipTopDirs,
                          const std::vector<std::string>& skipSuffixes,
                          std::string& error) {
        std::error_code ec;
        fs::create_directories(to, ec);

        fs::recursive_directory_iterator it(
            from, fs::directory_options::skip_permission_denied, ec);
        const fs::recursive_directory_iterator end;
        for (; !ec && it != end; it.increment(ec)) {
            const fs::path rel = fs::relative(it->path(), from, ec);
            if (ec) { error = "path resolution failed under " + from.string(); return false; }

            if (it->is_directory(ec)) {
                const std::string top = (rel.begin() == rel.end())
                    ? std::string() : rel.begin()->string();
                for (const auto& skip : skipTopDirs) {
                    if (top == skip) { it.disable_recursion_pending(); break; }
                }
                continue;
            }

            const std::string name = it->path().filename().string();
            bool skip = false;
            for (const auto& suffix : skipSuffixes) {
                if (name.size() >= suffix.size() && name.ends_with(suffix)) { skip = true; break; }
            }
            if (skip) continue;

            const fs::path dst = to / rel;
            fs::create_directories(dst.parent_path(), ec);
            fs::copy_file(it->path(), dst, fs::copy_options::overwrite_existing, ec);
            if (ec) { error = "copy failed: " + it->path().string(); return false; }
        }
        if (ec) { error = "scan failed under " + from.string(); return false; }
        return true;
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

        const fs::path outRoot = fs::path(opts.outputDir).lexically_normal() / project.name;
        if (fs::exists(outRoot, ec)) {
            fail("Output folder already exists: " + outRoot.string());
            return;
        }
        fs::create_directories(outRoot, ec);
        if (ec) { fail("Could not create " + outRoot.string()); return; }

        // ── 1. Gameplay module ──────────────────────────────────────────────
        setStage(PackageStage::BuildModule, 1, "Building the gameplay module");
        const fs::path slnx =
            fs::path(opts.projectRoot).lexically_normal() / (project.name + ".slnx");
        if (fs::exists(slnx, ec)) {
            const std::string msbuild = findMsbuild();
            if (msbuild.empty()) {
                fail("MSBuild was not found. Install Visual Studio with the C++ workload, "
                     "or build Binaries/game.dll yourself and package a project without a module.");
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

        // ── 2. Content ──────────────────────────────────────────────────────
        setStage(PackageStage::CollectContent, 2, "Collecting content");
        std::string err;
        const fs::path contentSrc = fs::path(opts.projectRoot).lexically_normal() / "content";
        if (fs::is_directory(contentSrc, ec)) {
            if (!copyTreeFiltered(contentSrc, outRoot / "content", {}, { ".camera.json" }, err)) {
                fail("Content copy failed: " + err);
                return;
            }
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
        const fs::path module =
            fs::path(opts.projectRoot).lexically_normal() / "Binaries" / "game.dll";
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
                              { "mcp", "temps" }, {}, err)) {
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
            if (!w.writeToFile((outRoot / "project.json").string())) {
                fail("Could not write project.json.");
                return;
            }

            JsonWriter b;
            b.set("engineVersion", kEngineVersion);
            b.set("configuration", opts.release ? std::string("Release") : std::string("Debug"));
            b.set("builtUtc", utcNow());
            b.writeToFile((outRoot / "build.json").string());
        }

        std::lock_guard<std::mutex> lock(g_mutex);
        g_state.stage   = PackageStage::Done;
        g_state.step    = g_state.stepCount;
        g_state.message = "Packaged: " + outRoot.string();
        g_state.detail  = outRoot.string();
        g_state.running = false;
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
