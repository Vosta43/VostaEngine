#include "vepch.h"
#include "Core/Diagnostics/Profiler.h"
#include "Core/Diagnostics/ProfilerExporter.h"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdint>
#include <ctime>
#include <filesystem>
#include <utility>

namespace ve::profiler {
namespace {

std::string baseName(const std::string& path) {
    const std::size_t slash = path.find_last_of("\\/");
    return slash == std::string::npos ? path : path.substr(slash + 1);
}

std::uint64_t nowNs() {
    return static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::nanoseconds>(
            std::chrono::steady_clock::now().time_since_epoch())
            .count());
}

} // namespace

Profiler& Profiler::Get() {
    static Profiler instance;
    return instance;
}

Profiler::Profiler() = default;

Profiler::~Profiler() {
    Stop();
    if (m_backend) {
        m_backend->shutdown();
        m_backend.reset();
    }
}

bool Profiler::Start(const CaptureOptions& options, std::string& error) {
    std::lock_guard<std::mutex> lock(m_lifecycle);
    if (m_running.load(std::memory_order_acquire)) {
        error = "profiler is already running";
        return false;
    }

    if (m_backend) {
        m_backend->shutdown();
        m_backend.reset();
    }

    m_options = options;
    m_options.sampleRateHz = std::clamp(options.sampleRateHz, 1u, 10000u);
    m_options.ringCapacity = std::clamp(options.ringCapacity, 1024u, 1000000u);

    m_backend = createPlatformBackend();
    if (!m_backend) {
        error = "no sampling backend for this platform";
        return false;
    }
    if (!m_backend->initialize(error)) {
        m_backend.reset();
        return false;
    }

    m_modules.clear();
    if (!m_backend->snapshotModules(m_modules, error)) {
        m_backend->shutdown();
        m_backend.reset();
        return false;
    }

    m_ring.assign(m_options.ringCapacity, Sample{});
    m_writeCount.store(0, std::memory_order_relaxed);
    m_failedCaptures.store(0, std::memory_order_relaxed);
    m_depthSum.store(0, std::memory_order_relaxed);
    m_maxSuspendMicros.store(0, std::memory_order_relaxed);
    m_symbolCache.clear();
    m_samplerThreadId = 0;

    refreshThreads();

    m_stopRequested.store(false, std::memory_order_release);
    m_running.store(true, std::memory_order_release);
    m_samplerThread = std::thread(&Profiler::samplerLoop, this);
    return true;
}

void Profiler::Stop() {
    std::lock_guard<std::mutex> lock(m_lifecycle);
    if (!m_running.load(std::memory_order_acquire)) {
        return;
    }
    m_stopRequested.store(true, std::memory_order_release);
    if (m_backend) {
        m_backend->wake();
    }
    if (m_samplerThread.joinable()) {
        m_samplerThread.join();
    }
    m_running.store(false, std::memory_order_release);
}

bool Profiler::IsRunning() const {
    return m_running.load(std::memory_order_acquire);
}

void Profiler::refreshThreads() {
    if (!m_options.threadIds.empty()) {
        m_threads.clear();
        m_threads.reserve(m_options.threadIds.size());
        for (const std::uint64_t id : m_options.threadIds) {
            ThreadInfo info;
            info.id = id;
            info.name = "Thread " + std::to_string(id);
            m_threads.push_back(std::move(info));
        }
        return;
    }

    std::vector<ThreadInfo> found;
    std::string error;
    if (!m_backend || !m_backend->enumerateThreads(found, error)) {
        return;
    }

    m_threads.clear();
    m_threads.reserve(found.size());
    for (ThreadInfo& info : found) {
        if (info.id == 0 || info.id == m_samplerThreadId) {
            continue;
        }
        m_threads.push_back(std::move(info));
    }
}

void Profiler::samplerLoop() {
    m_samplerThreadId = m_backend->currentThreadId();
    refreshThreads();

    const auto interval = std::chrono::nanoseconds(
        1'000'000'000ll / static_cast<long long>(m_options.sampleRateHz));

    std::size_t cursor = 0;
    auto lastRefresh = std::chrono::steady_clock::now();
    auto nextTick = lastRefresh;

    while (!m_stopRequested.load(std::memory_order_acquire)) {
        const auto tickStart = std::chrono::steady_clock::now();
        if (m_options.threadIds.empty() && tickStart - lastRefresh >= std::chrono::seconds(1)) {
            refreshThreads();
            lastRefresh = tickStart;
            cursor = 0;
        }

        if (!m_threads.empty()) {
            const std::uint64_t threadId = m_threads[cursor % m_threads.size()].id;
            ++cursor;

            Frame frames[kMaxFrames];
            std::uint32_t suspendMicros = 0;
            const std::uint32_t depth = m_backend->captureStack(
                threadId, frames, kMaxFrames, suspendMicros);

            std::uint32_t previous = m_maxSuspendMicros.load(std::memory_order_relaxed);
            while (suspendMicros > previous &&
                   !m_maxSuspendMicros.compare_exchange_weak(
                       previous, suspendMicros, std::memory_order_relaxed)) {
            }

            if (depth == 0) {
                m_failedCaptures.fetch_add(1, std::memory_order_relaxed);
            } else {
                Sample sample;
                sample.threadId = threadId;
                sample.timestampNs = nowNs();
                sample.frameCount = depth;
                for (std::uint32_t i = 0; i < depth; ++i) {
                    sample.frames[i] = frames[i];
                }

                const std::uint64_t index =
                    m_writeCount.fetch_add(1, std::memory_order_relaxed);
                m_ring[index % m_ring.size()] = sample;
                m_depthSum.fetch_add(depth, std::memory_order_relaxed);
            }
        }

        nextTick += interval;
        const auto now = std::chrono::steady_clock::now();
        if (nextTick < now) {
            if (now - nextTick > interval) {
                nextTick = now;
            }
        } else {
            m_backend->waitForInterval(
                std::chrono::duration_cast<std::chrono::nanoseconds>(nextTick - now),
                m_stopRequested);
        }
    }
}

ResolvedFrame Profiler::resolveFrame(const Frame& frame, bool& resolved) {
    const std::uint64_t key =
        (static_cast<std::uint64_t>(frame.module) << 32) | frame.rva;

    const auto cached = m_symbolCache.find(key);
    if (cached != m_symbolCache.end()) {
        resolved = true;
        return cached->second;
    }

    ResolvedFrame output;
    bool ok = false;
    if (frame.module < m_modules.size() && m_backend) {
        ok = m_backend->symbolize(m_modules[frame.module], frame.rva, output);
    }

    if (!ok || output.function.empty()) {
        const std::string moduleName =
            frame.module < m_modules.size() ? baseName(m_modules[frame.module].path)
                                            : std::string("unknown");
        char buffer[64]{};
        std::snprintf(buffer, sizeof(buffer), "0x%08X", frame.rva);
        output.function = moduleName + "+" + buffer;
    }

    m_symbolCache.emplace(key, output);
    resolved = ok;
    return output;
}

bool Profiler::ComputeSelfTime(std::vector<SelfTimeEntry>& out, std::string& error) {
    if (m_running.load(std::memory_order_acquire)) {
        error = "stop the profiler before computing self time";
        return false;
    }
    if (!m_backend) {
        error = "no capture to analyze";
        return false;
    }

    const std::uint64_t total = m_writeCount.load(std::memory_order_relaxed);
    const std::uint64_t count = std::min<std::uint64_t>(total, m_ring.size());
    if (count == 0) {
        error = "capture contains no samples";
        return false;
    }

    std::unordered_map<std::string, std::uint64_t> counts;
    for (std::uint64_t i = total - count; i < total; ++i) {
        const Sample& sample = m_ring[i % m_ring.size()];
        if (sample.frameCount == 0) {
            continue;
        }
        bool resolved = false;
        const ResolvedFrame frame = resolveFrame(sample.frames[0], resolved);
        ++counts[frame.function];
    }

    out.clear();
    out.reserve(counts.size());
    for (const auto& entry : counts) {
        SelfTimeEntry item;
        item.function = entry.first;
        item.samples = entry.second;
        item.percent =
            100.0 * static_cast<double>(entry.second) / static_cast<double>(count);
        out.push_back(std::move(item));
    }

    std::sort(out.begin(), out.end(),
              [](const SelfTimeEntry& a, const SelfTimeEntry& b) {
                  return a.samples > b.samples;
              });
    return true;
}

bool Profiler::WriteSpeedscope(const std::string& path, std::string& error) {
    if (m_running.load(std::memory_order_acquire)) {
        error = "stop the profiler before exporting";
        return false;
    }
    if (!m_backend) {
        error = "no capture to export";
        return false;
    }

    const std::uint64_t total = m_writeCount.load(std::memory_order_relaxed);
    if (total == 0) {
        error = "capture contains no samples";
        return false;
    }

    FrameResolver resolver = [this](std::uint32_t module, std::uint32_t rva) {
        Frame frame;
        frame.module = module;
        frame.rva = rva;
        bool resolved = false;
        return resolveFrame(frame, resolved);
    };

    return writeSpeedscopeFile(path, m_ring, total, m_modules, m_threads,
                               m_options.sampleRateHz, resolver, error);
}

bool Profiler::WriteCaptureFile(const std::string& directory,
                                std::string& outPath, std::string& error) {
    std::error_code ec;
    std::filesystem::create_directories(directory, ec);
    if (ec) {
        error = "failed to create capture directory: " + ec.message();
        return false;
    }

    char stamp[32]{};
    const std::time_t now = std::time(nullptr);
    std::tm local{};
#if defined(_WIN32)
    localtime_s(&local, &now);
#else
    localtime_r(&now, &local);
#endif
    std::strftime(stamp, sizeof(stamp), "%Y%m%d_%H%M%S", &local);
    outPath = (std::filesystem::path(directory) /
               (std::string("cpu_") + stamp + ".speedscope.json"))
                  .string();
    return WriteSpeedscope(outPath, error);
}

ProfilerStats Profiler::Stats() const {
    ProfilerStats stats;
    stats.samples = m_writeCount.load(std::memory_order_relaxed);
    stats.failedCaptures = m_failedCaptures.load(std::memory_order_relaxed);
    stats.maxSuspendMicros = m_maxSuspendMicros.load(std::memory_order_relaxed);
    if (stats.samples > 0) {
        stats.averageDepth = static_cast<double>(
                                 m_depthSum.load(std::memory_order_relaxed)) /
                             static_cast<double>(stats.samples);
    }
    return stats;
}

const CaptureOptions& Profiler::Options() const {
    return m_options;
}

const std::vector<ModuleInfo>& Profiler::Modules() const {
    return m_modules;
}

const std::vector<ThreadInfo>& Profiler::Threads() const {
    return m_threads;
}

} // namespace ve::profiler
