#pragma once

#include "Core/Core.h"
#include "Core/Diagnostics/ProfilerBackend.h"
#include "Core/Diagnostics/ProfilerTypes.h"

#include <atomic>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

#if defined(VE_PROFILER_STATIC)
#define VE_PROFILER_API
#else
#define VE_PROFILER_API VE_API
#endif

namespace ve::profiler {

class VE_PROFILER_API Profiler {
public:
    static Profiler& Get();

    bool Start(const CaptureOptions& options, std::string& error);
    void Stop();
    bool IsRunning() const;

    bool WriteSpeedscope(const std::string& path, std::string& error);
    // Writes a timestamped capture into directory; outPath receives the file
    // that was created.
    bool WriteCaptureFile(const std::string& directory, std::string& outPath,
                          std::string& error);
    bool ComputeSelfTime(std::vector<SelfTimeEntry>& out, std::string& error);

    ProfilerStats Stats() const;
    const CaptureOptions& Options() const;
    const std::vector<ModuleInfo>& Modules() const;
    const std::vector<ThreadInfo>& Threads() const;

    Profiler(const Profiler&) = delete;
    Profiler& operator=(const Profiler&) = delete;

private:
    Profiler();
    ~Profiler();

    void samplerLoop();
    void refreshThreads();
    ResolvedFrame resolveFrame(const Frame& frame, bool& resolved);

    std::mutex m_lifecycle;
    std::unique_ptr<IProfilerBackend> m_backend;
    CaptureOptions m_options;
    std::vector<ModuleInfo> m_modules;
    std::vector<ThreadInfo> m_threads;
    std::vector<Sample> m_ring;
    std::atomic<std::uint64_t> m_writeCount{0};
    std::atomic<std::uint64_t> m_failedCaptures{0};
    std::atomic<std::uint64_t> m_depthSum{0};
    std::atomic<std::uint32_t> m_maxSuspendMicros{0};
    std::atomic<bool> m_running{false};
    std::atomic<bool> m_stopRequested{false};
    std::thread m_samplerThread;
    std::uint64_t m_samplerThreadId = 0;
    std::unordered_map<std::uint64_t, ResolvedFrame> m_symbolCache;
};

} // namespace ve::profiler
