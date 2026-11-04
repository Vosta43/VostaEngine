#include "Core/Diagnostics/Profiler.h"

#include <windows.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <thread>
#include <vector>

namespace {

using Clock = std::chrono::steady_clock;

std::atomic<std::uint64_t> g_sink{1};
std::atomic<std::uint64_t> g_workerThreadId{0};
std::atomic<bool> g_workerRunning{false};

__declspec(noinline) void hot_a(std::uint32_t iterations) {
    std::uint64_t value = g_sink.load(std::memory_order_relaxed) | 1ull;
    for (std::uint32_t i = 0; i < iterations; ++i) {
        value ^= value << 13;
        value ^= value >> 7;
        value ^= value << 17;
    }
    g_sink.store(value, std::memory_order_relaxed);
}

__declspec(noinline) void hot_b(std::uint32_t iterations) {
    std::uint64_t value =
        g_sink.load(std::memory_order_relaxed) + 0x9E3779B97F4A7C15ull;
    for (std::uint32_t i = 0; i < iterations; ++i) {
        value = value * 6364136223846793005ull + 1442695040888963407ull;
    }
    g_sink.store(value, std::memory_order_relaxed);
}

void worker(std::chrono::milliseconds duration) {
    g_workerThreadId.store(GetCurrentThreadId(), std::memory_order_release);
    g_workerRunning.store(true, std::memory_order_release);

    const auto end = Clock::now() + duration;
    while (Clock::now() < end) {
        const auto phaseEnd = Clock::now() + std::chrono::milliseconds(50);
        while (Clock::now() < phaseEnd) {
            hot_a(4096);
        }
        const auto hotBEnd = Clock::now() + std::chrono::milliseconds(50);
        while (Clock::now() < hotBEnd) {
            hot_b(4096);
        }
    }

    g_workerRunning.store(false, std::memory_order_release);
}

double findPercent(const std::vector<ve::profiler::SelfTimeEntry>& entries,
                   const char* needle) {
    for (const auto& entry : entries) {
        if (entry.function.find(needle) != std::string::npos) {
            return entry.percent;
        }
    }
    return 0.0;
}

} // namespace

int main(int argc, char** argv) {
    std::uint32_t seconds = 5;
    std::uint32_t rate = 1000;
    std::string outputPath = "profiler_smoke.speedscope.json";

    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--seconds") == 0 && i + 1 < argc) {
            seconds = static_cast<std::uint32_t>(std::atoi(argv[++i]));
        } else if (std::strcmp(argv[i], "--rate") == 0 && i + 1 < argc) {
            rate = static_cast<std::uint32_t>(std::atoi(argv[++i]));
        } else if (std::strcmp(argv[i], "--out") == 0 && i + 1 < argc) {
            outputPath = argv[++i];
        }
    }

    if (seconds == 0) {
        seconds = 1;
    }
    if (rate == 0) {
        rate = 1000;
    }

    std::thread workerThread(worker, std::chrono::milliseconds(seconds * 1000));

    while (g_workerThreadId.load(std::memory_order_acquire) == 0) {
        std::this_thread::yield();
    }
    const std::uint64_t targetThread =
        g_workerThreadId.load(std::memory_order_acquire);

    ve::profiler::CaptureOptions options;
    options.sampleRateHz = rate;
    options.ringCapacity = std::max<std::uint32_t>(8192, rate * seconds * 2);
    options.threadIds = {targetThread};

    std::string error;
    if (!ve::profiler::Profiler::Get().Start(options, error)) {
        std::printf("Start failed: %s\n", error.c_str());
        workerThread.join();
        return 1;
    }

    while (g_workerRunning.load(std::memory_order_acquire)) {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }

    ve::profiler::Profiler::Get().Stop();
    workerThread.join();

    const ve::profiler::ProfilerStats stats = ve::profiler::Profiler::Get().Stats();
    const double failedPercent =
        stats.samples + stats.failedCaptures == 0
            ? 0.0
            : 100.0 * static_cast<double>(stats.failedCaptures) /
                  static_cast<double>(stats.samples + stats.failedCaptures);

    std::printf("samples=%llu failed=%llu (%.2f%%) maxSuspend=%uus avgDepth=%.1f\n",
                static_cast<unsigned long long>(stats.samples),
                static_cast<unsigned long long>(stats.failedCaptures),
                failedPercent, stats.maxSuspendMicros, stats.averageDepth);

    if (!ve::profiler::Profiler::Get().WriteSpeedscope(outputPath, error)) {
        std::printf("WriteSpeedscope failed: %s\n", error.c_str());
        return 1;
    }
    std::printf("wrote %s\n", outputPath.c_str());

    std::vector<ve::profiler::SelfTimeEntry> selfTime;
    if (!ve::profiler::Profiler::Get().ComputeSelfTime(selfTime, error)) {
        std::printf("ComputeSelfTime failed: %s\n", error.c_str());
        return 1;
    }

    std::printf("top functions (self time):\n");
    for (std::size_t i = 0; i < selfTime.size() && i < 8; ++i) {
        std::printf("  %6.2f%%  %s\n", selfTime[i].percent,
                    selfTime[i].function.c_str());
    }

    const double percentA = findPercent(selfTime, "hot_a");
    const double percentB = findPercent(selfTime, "hot_b");
    std::printf("hot_a=%.2f%% hot_b=%.2f%%\n", percentA, percentB);

    // Regression: with no explicit thread list the sampler enumerates every
    // thread, itself included. SuspendThread on self never returns, so the
    // sampler deadlocks and the next Stop() hangs on join.
    ve::profiler::CaptureOptions allThreads;
    allThreads.sampleRateHz = rate;
    allThreads.ringCapacity = std::max<std::uint32_t>(8192, rate * 4);

    std::printf("all-threads capture (Stop must return)...\n");
    std::fflush(stdout);
    if (!ve::profiler::Profiler::Get().Start(allThreads, error)) {
        std::printf("all-threads Start failed: %s\n", error.c_str());
        return 1;
    }
    const auto spinEnd = Clock::now() + std::chrono::milliseconds(500);
    while (Clock::now() < spinEnd) {
        hot_a(4096);
    }
    ve::profiler::Profiler::Get().Stop();

    const ve::profiler::ProfilerStats allStats =
        ve::profiler::Profiler::Get().Stats();
    bool sawMainThread = false;
    for (const auto& info : ve::profiler::Profiler::Get().Threads()) {
        if (info.id == GetCurrentThreadId()) {
            sawMainThread = true;
        }
    }
    std::printf("all-threads samples=%llu failed=%llu main=%d\n",
                static_cast<unsigned long long>(allStats.samples),
                static_cast<unsigned long long>(allStats.failedCaptures),
                sawMainThread ? 1 : 0);
    const bool allThreadsOk = allStats.samples > 0 && sawMainThread;

    const bool pass = allThreadsOk &&
                      percentA > 35.0 && percentA < 65.0 &&
                      percentB > 35.0 && percentB < 65.0 &&
                      failedPercent < 5.0;
    std::printf("%s\n", pass ? "PASS" : "FAIL");
    if (!pass) {
        std::printf(
            "If hot_a/hot_b are missing, check that ProfilerSmoke.pdb sits "
            "next to the executable.\n");
    }
    return pass ? 0 : 1;
}
