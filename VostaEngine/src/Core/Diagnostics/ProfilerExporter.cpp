#include "vepch.h"
#include "Core/Diagnostics/ProfilerExporter.h"

#include <algorithm>
#include <cstdio>
#include <fstream>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace ve::profiler {
namespace {

std::string escapeJson(const std::string& value) {
    std::string out;
    out.reserve(value.size() + 8);
    for (const unsigned char c : value) {
        switch (c) {
        case '"': out += "\\\""; break;
        case '\\': out += "\\\\"; break;
        case '\b': out += "\\b"; break;
        case '\f': out += "\\f"; break;
        case '\n': out += "\\n"; break;
        case '\r': out += "\\r"; break;
        case '\t': out += "\\t"; break;
        default:
            if (c < 0x20) {
                char buffer[8]{};
                std::snprintf(buffer, sizeof(buffer), "\\u%04x", c);
                out += buffer;
            } else {
                out += static_cast<char>(c);
            }
        }
    }
    return out;
}

struct ExportedFrame {
    std::string name;
    std::string file;
    std::uint32_t line = 0;
};

struct ExportedProfile {
    std::uint64_t threadId = 0;
    std::vector<std::vector<std::uint32_t>> stacks;
};

std::string threadName(const std::vector<ThreadInfo>& threads, std::uint64_t id) {
    for (const ThreadInfo& info : threads) {
        if (info.id == id && !info.name.empty()) {
            return info.name;
        }
    }
    return "Thread " + std::to_string(id);
}

} // namespace

bool writeSpeedscopeFile(const std::string& path,
                         const std::vector<Sample>& ring,
                         std::uint64_t writeCount,
                         const std::vector<ModuleInfo>& modules,
                         const std::vector<ThreadInfo>& threads,
                         std::uint32_t sampleRateHz,
                         const FrameResolver& resolver,
                         std::string& error) {
    (void)modules;

    if (ring.empty()) {
        error = "empty sample ring";
        return false;
    }

    const std::uint64_t count = std::min<std::uint64_t>(writeCount, ring.size());
    if (count == 0) {
        error = "capture contains no samples";
        return false;
    }

    const std::uint64_t weight =
        std::max<std::uint64_t>(1ull, 1'000'000'000ull / sampleRateHz);

    std::vector<ExportedFrame> frames;
    std::unordered_map<std::string, std::uint32_t> frameIndex;

    std::vector<ExportedProfile> profiles;
    std::unordered_map<std::uint64_t, std::size_t> profileIndex;

    for (std::uint64_t i = writeCount - count; i < writeCount; ++i) {
        const Sample& sample = ring[i % ring.size()];
        if (sample.frameCount == 0) {
            continue;
        }

        auto profileEntry = profileIndex.find(sample.threadId);
        if (profileEntry == profileIndex.end()) {
            profileEntry = profileIndex.emplace(sample.threadId, profiles.size()).first;
            ExportedProfile profile;
            profile.threadId = sample.threadId;
            profiles.push_back(std::move(profile));
        }

        std::vector<std::uint32_t> stack;
        stack.reserve(sample.frameCount);
        for (std::uint32_t f = sample.frameCount; f > 0; --f) {
            const Frame& frame = sample.frames[f - 1];
            const ResolvedFrame resolved = resolver(frame.module, frame.rva);
            if (resolved.function.empty()) {
                continue;
            }

            auto entry = frameIndex.find(resolved.function);
            if (entry == frameIndex.end()) {
                entry = frameIndex.emplace(resolved.function,
                                           static_cast<std::uint32_t>(frames.size()))
                            .first;
                ExportedFrame exported;
                exported.name = resolved.function;
                exported.file = resolved.file;
                exported.line = resolved.line;
                frames.push_back(std::move(exported));
            }
            stack.push_back(entry->second);
        }

        if (!stack.empty()) {
            profiles[profileEntry->second].stacks.push_back(std::move(stack));
        }
    }

    std::string json;
    json.reserve(64 * 1024);
    json += "{\"$schema\":\"https://www.speedscope.app/file-format-schema.json\",";
    json += "\"shared\":{\"frames\":[";

    for (std::size_t i = 0; i < frames.size(); ++i) {
        if (i != 0) {
            json += ',';
        }
        json += "{\"name\":\"";
        json += escapeJson(frames[i].name);
        json += '"';
        if (!frames[i].file.empty()) {
            json += ",\"file\":\"";
            json += escapeJson(frames[i].file);
            json += '"';
            if (frames[i].line != 0) {
                json += ",\"line\":";
                json += std::to_string(frames[i].line);
            }
        }
        json += '}';
    }

    json += "]},\"profiles\":[";
    bool firstProfile = true;
    for (const ExportedProfile& profile : profiles) {
        if (profile.stacks.empty()) {
            continue;
        }
        if (!firstProfile) {
            json += ',';
        }
        firstProfile = false;

        json += "{\"type\":\"sampled\",\"name\":\"";
        json += escapeJson(threadName(threads, profile.threadId));
        json += "\",\"unit\":\"nanoseconds\",\"startValue\":0,\"endValue\":";
        json += std::to_string(profile.stacks.size() * weight);
        json += ",\"samples\":[";

        for (std::size_t s = 0; s < profile.stacks.size(); ++s) {
            if (s != 0) {
                json += ',';
            }
            json += '[';
            const std::vector<std::uint32_t>& stack = profile.stacks[s];
            for (std::size_t f = 0; f < stack.size(); ++f) {
                if (f != 0) {
                    json += ',';
                }
                json += std::to_string(stack[f]);
            }
            json += ']';
        }

        json += "],\"weights\":[";
        for (std::size_t s = 0; s < profile.stacks.size(); ++s) {
            if (s != 0) {
                json += ',';
            }
            json += std::to_string(weight);
        }
        json += "]}";
    }

    json += "],\"activeProfileIndex\":0,";
    json += "\"exporter\":\"VostaEngine Profiler\",";
    json += "\"name\":\"VostaEngine CPU capture\"}";

    std::ofstream file(path, std::ios::binary | std::ios::trunc);
    if (!file) {
        error = "cannot open output file: " + path;
        return false;
    }
    file.write(json.data(), static_cast<std::streamsize>(json.size()));
    if (!file) {
        error = "failed to write output file: " + path;
        return false;
    }
    return true;
}

} // namespace ve::profiler
