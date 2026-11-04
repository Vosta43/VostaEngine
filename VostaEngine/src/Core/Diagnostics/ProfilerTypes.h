#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace ve::profiler {

inline constexpr std::uint32_t kMaxFrames = 64;
inline constexpr std::uint32_t kBuildIdSize = 20;
inline constexpr std::uint32_t kInvalidModule = 0xFFFFFFFFu;

struct Frame {
    std::uint32_t module = kInvalidModule;
    std::uint32_t rva = 0;
};

struct Sample {
    std::uint64_t threadId = 0;
    std::uint64_t timestampNs = 0;
    std::uint32_t frameCount = 0;
    Frame frames[kMaxFrames]{};
};

struct ThreadInfo {
    std::uint64_t id = 0;
    std::string name;
};

struct ModuleInfo {
    std::uint64_t base = 0;
    std::uint64_t size = 0;
    std::string path;
    std::uint8_t buildId[kBuildIdSize]{};
    std::uint32_t buildIdSize = 0;
};

struct ResolvedFrame {
    std::string function;
    std::string file;
    std::uint32_t line = 0;
};

struct SelfTimeEntry {
    std::string function;
    std::uint64_t samples = 0;
    double percent = 0.0;
};

struct CaptureOptions {
    std::uint32_t sampleRateHz = 1000;
    std::uint32_t ringCapacity = 32768;
    std::vector<std::uint64_t> threadIds;
};

struct ProfilerStats {
    std::uint64_t samples = 0;
    std::uint64_t failedCaptures = 0;
    std::uint32_t maxSuspendMicros = 0;
    double averageDepth = 0.0;
};

} // namespace ve::profiler
