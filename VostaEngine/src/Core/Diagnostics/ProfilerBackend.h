#pragma once

#include "Core/Diagnostics/ProfilerTypes.h"

#include <atomic>
#include <chrono>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace ve::profiler {

class IProfilerBackend {
public:
    virtual ~IProfilerBackend() = default;

    virtual bool initialize(std::string& error) = 0;
    virtual void shutdown() = 0;

    virtual bool enumerateThreads(std::vector<ThreadInfo>& out, std::string& error) = 0;
    virtual std::uint32_t captureStack(std::uint64_t threadId, Frame* frames,
                                       std::uint32_t capacity,
                                       std::uint32_t& suspendMicros) = 0;
    virtual bool snapshotModules(std::vector<ModuleInfo>& out, std::string& error) = 0;
    virtual bool symbolize(const ModuleInfo& module, std::uint32_t rva,
                           ResolvedFrame& out) = 0;

    virtual std::uint64_t currentThreadId() const = 0;
    virtual void waitForInterval(std::chrono::nanoseconds interval,
                                 const std::atomic<bool>& stop) = 0;
    virtual void wake() = 0;
};

std::unique_ptr<IProfilerBackend> createPlatformBackend();

} // namespace ve::profiler
