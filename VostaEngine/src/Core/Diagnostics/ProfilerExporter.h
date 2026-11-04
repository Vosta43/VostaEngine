#pragma once

#include "Core/Diagnostics/ProfilerTypes.h"

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace ve::profiler {

using FrameResolver = std::function<ResolvedFrame(std::uint32_t module, std::uint32_t rva)>;

bool writeSpeedscopeFile(const std::string& path,
                         const std::vector<Sample>& ring,
                         std::uint64_t writeCount,
                         const std::vector<ModuleInfo>& modules,
                         const std::vector<ThreadInfo>& threads,
                         std::uint32_t sampleRateHz,
                         const FrameResolver& resolver,
                         std::string& error);

} // namespace ve::profiler
