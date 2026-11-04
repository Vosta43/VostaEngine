#if !defined(_WIN32) || !defined(_M_X64)
#error "ProfilerBackendWin64.cpp is Windows x64 only"
#endif

#include "vepch.h"
#include "Core/Diagnostics/ProfilerBackend.h"

#include <windows.h>
#include <dbghelp.h>
#include <tlhelp32.h>
#include <winternl.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstring>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <utility>
#include <vector>

#ifndef CREATE_WAITABLE_TIMER_HIGH_RESOLUTION
#define CREATE_WAITABLE_TIMER_HIGH_RESOLUTION 0x00000002
#endif

extern "C" NTSTATUS NTAPI NtQueryInformationThread(HANDLE ThreadHandle,
                                                   THREADINFOCLASS ThreadInformationClass,
                                                   PVOID ThreadInformation,
                                                   ULONG ThreadInformationLength,
                                                   PULONG ReturnLength);

namespace ve::profiler {
namespace {

// Not exposed by every Windows SDK version's winternl.h.
struct ThreadBasicInformationData {
    NTSTATUS ExitStatus;
    PVOID TebBaseAddress;
    CLIENT_ID ClientId;
    KAFFINITY AffinityMask;
    KPRIORITY Priority;
    KPRIORITY BasePriority;
};

std::mutex g_symbolMutex;
bool g_symbolsInitialized = false;

std::string utf8FromWide(const wchar_t* text) {
    if (text == nullptr || text[0] == L'\0') {
        return {};
    }
    const int length =
        WideCharToMultiByte(CP_UTF8, 0, text, -1, nullptr, 0, nullptr, nullptr);
    if (length <= 1) {
        return {};
    }
    std::string output(static_cast<std::size_t>(length - 1), '\0');
    WideCharToMultiByte(CP_UTF8, 0, text, -1, output.data(), length, nullptr,
                        nullptr);
    return output;
}

std::string utf8FromWide(const std::wstring& text) {
    return utf8FromWide(text.c_str());
}

std::wstring wideFromUtf8(const std::string& text) {
    if (text.empty()) {
        return {};
    }
    const int length = MultiByteToWideChar(
        CP_UTF8, 0, text.data(), static_cast<int>(text.size()), nullptr, 0);
    if (length <= 0) {
        return {};
    }
    std::wstring output(static_cast<std::size_t>(length), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()),
                        output.data(), length);
    return output;
}

std::wstring directoryOf(const std::wstring& path) {
    const std::size_t slash = path.find_last_of(L"\\/");
    return slash == std::wstring::npos ? std::wstring() : path.substr(0, slash);
}

std::string threadName(HANDLE thread, DWORD id) {
    using GetThreadDescriptionFn = HRESULT(WINAPI*)(HANDLE, PWSTR*);
    static const auto getThreadDescription =
        reinterpret_cast<GetThreadDescriptionFn>(GetProcAddress(
            GetModuleHandleW(L"kernel32.dll"), "GetThreadDescription"));

    if (getThreadDescription != nullptr) {
        PWSTR description = nullptr;
        if (SUCCEEDED(getThreadDescription(thread, &description)) &&
            description != nullptr) {
            std::string name = utf8FromWide(description);
            LocalFree(description);
            if (!name.empty()) {
                return name;
            }
        }
    }
    return "Thread " + std::to_string(id);
}

} // namespace

class WindowsBackend final : public IProfilerBackend {
public:
    bool initialize(std::string& error) override {
        m_timer = CreateWaitableTimerExW(nullptr, nullptr,
                                         CREATE_WAITABLE_TIMER_HIGH_RESOLUTION,
                                         TIMER_ALL_ACCESS);
        if (m_timer == nullptr) {
            m_timer = CreateWaitableTimerW(nullptr, FALSE, nullptr);
        }
        if (m_timer == nullptr) {
            error = "CreateWaitableTimer failed";
            return false;
        }

        m_wakeEvent = CreateEventW(nullptr, FALSE, FALSE, nullptr);
        if (m_wakeEvent == nullptr) {
            error = "CreateEvent failed";
            CloseHandle(m_timer);
            m_timer = nullptr;
            return false;
        }
        return true;
    }

    void shutdown() override {
        if (m_timer != nullptr) {
            CloseHandle(m_timer);
            m_timer = nullptr;
        }
        if (m_wakeEvent != nullptr) {
            CloseHandle(m_wakeEvent);
            m_wakeEvent = nullptr;
        }
    }

    bool enumerateThreads(std::vector<ThreadInfo>& out,
                          std::string& error) override {
        const HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
        if (snapshot == INVALID_HANDLE_VALUE) {
            error = "CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD) failed";
            return false;
        }

        const DWORD processId = GetCurrentProcessId();
        THREADENTRY32 entry{};
        entry.dwSize = sizeof(entry);

        out.clear();
        if (Thread32First(snapshot, &entry)) {
            do {
                if (entry.th32OwnerProcessID != processId ||
                    entry.th32ThreadID == 0) {
                    continue;
                }

                ThreadInfo info;
                info.id = entry.th32ThreadID;
                const HANDLE thread =
                    OpenThread(THREAD_QUERY_LIMITED_INFORMATION, FALSE,
                               entry.th32ThreadID);
                if (thread != nullptr) {
                    info.name = threadName(thread, entry.th32ThreadID);
                    CloseHandle(thread);
                } else {
                    info.name = "Thread " + std::to_string(entry.th32ThreadID);
                }
                out.push_back(std::move(info));
            } while (Thread32Next(snapshot, &entry));
        }

        CloseHandle(snapshot);
        std::sort(out.begin(), out.end(),
                  [](const ThreadInfo& a, const ThreadInfo& b) {
                      return a.id < b.id;
                  });
        return true;
    }

    std::uint32_t captureStack(std::uint64_t threadId, Frame* frames,
                               std::uint32_t capacity,
                               std::uint32_t& suspendMicros) override {
        suspendMicros = 0;

        // SuspendThread on the calling thread blocks forever; never target self.
        if (threadId == GetCurrentThreadId()) {
            return 0;
        }

        const HANDLE thread = OpenThread(
            THREAD_SUSPEND_RESUME | THREAD_GET_CONTEXT | THREAD_QUERY_INFORMATION,
            FALSE, static_cast<DWORD>(threadId));
        if (thread == nullptr) {
            return 0;
        }

        std::uint64_t stackLow = 0;
        std::uint64_t stackHigh = 0;
        const bool haveLimits = stackLimits(thread, stackLow, stackHigh);

        const auto suspendStart = std::chrono::steady_clock::now();
        if (SuspendThread(thread) == static_cast<DWORD>(-1)) {
            CloseHandle(thread);
            return 0;
        }

        CONTEXT context{};
        context.ContextFlags = CONTEXT_CONTROL | CONTEXT_INTEGER;
        const BOOL captured = GetThreadContext(thread, &context);
        ResumeThread(thread);

        const auto suspendEnd = std::chrono::steady_clock::now();
        suspendMicros = static_cast<std::uint32_t>(
            std::chrono::duration_cast<std::chrono::microseconds>(suspendEnd -
                                                                  suspendStart)
                .count());
        CloseHandle(thread);

        if (!captured) {
            return 0;
        }

        std::uint32_t depth = 0;
        std::uint64_t rip = context.Rip;
        std::uint64_t rsp = context.Rsp;
        std::uint64_t previousRsp = 0;
        bool havePrevious = false;

        auto appendFrame = [&](std::uint64_t ip) -> bool {
            std::uint32_t module = 0;
            std::uint32_t rva = 0;
            if (!mapAddress(ip, true, module, rva)) {
                return false;
            }
            if (depth < capacity) {
                frames[depth] = Frame{module, rva};
                ++depth;
            }
            return true;
        };

        if (!appendFrame(rip)) {
            return 0;
        }

        while (depth < capacity) {
            if (havePrevious && rsp <= previousRsp) {
                break;
            }
            if (haveLimits && (rsp < stackLow || rsp + sizeof(std::uint64_t) > stackHigh)) {
                break;
            }
            previousRsp = rsp;
            havePrevious = true;

            DWORD64 imageBase = 0;
            const PRUNTIME_FUNCTION entry =
                RtlLookupFunctionEntry(static_cast<DWORD64>(rip), &imageBase,
                                       nullptr);

            if (entry == nullptr) {
                if (haveLimits &&
                    rsp + sizeof(std::uint64_t) > stackHigh) {
                    break;
                }
                const auto returnAddress =
                    *reinterpret_cast<const std::uint64_t*>(rsp);
                if (!appendFrame(returnAddress)) {
                    break;
                }
                rip = returnAddress;
                rsp += sizeof(std::uint64_t);
            } else {
                CONTEXT caller = context;
                PVOID handlerData = nullptr;
                DWORD64 establisherFrame = 0;
                KNONVOLATILE_CONTEXT_POINTERS nonvolatile = {};
                RtlVirtualUnwind(UNW_FLAG_NHANDLER, imageBase,
                                 static_cast<DWORD64>(rip), entry, &caller,
                                 &handlerData, &establisherFrame,
                                 &nonvolatile);
                rip = caller.Rip;
                rsp = caller.Rsp;
                context = caller;
                if (rip == 0) {
                    break;
                }
                if (!appendFrame(rip)) {
                    break;
                }
            }
        }

        return depth;
    }

    bool snapshotModules(std::vector<ModuleInfo>& out,
                         std::string& error) override {
        const HANDLE snapshot = CreateToolhelp32Snapshot(
            TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, GetCurrentProcessId());
        if (snapshot == INVALID_HANDLE_VALUE) {
            error = "CreateToolhelp32Snapshot(TH32CS_SNAPMODULE) failed";
            return false;
        }

        MODULEENTRY32W entry{};
        entry.dwSize = sizeof(entry);

        m_modules.clear();
        if (Module32FirstW(snapshot, &entry)) {
            do {
                ModuleRecord record;
                record.info.base =
                    reinterpret_cast<std::uint64_t>(entry.modBaseAddr);
                record.info.size = entry.modBaseSize;
                record.info.path = utf8FromWide(entry.szExePath);
                readImageMetadata(record);
                m_modules.push_back(std::move(record));
            } while (Module32NextW(snapshot, &entry));
        }
        CloseHandle(snapshot);

        std::sort(m_modules.begin(), m_modules.end(),
                  [](const ModuleRecord& a, const ModuleRecord& b) {
                      return a.info.base < b.info.base;
                  });

        out.clear();
        out.reserve(m_modules.size());
        for (const ModuleRecord& record : m_modules) {
            out.push_back(record.info);
        }
        return true;
    }

    bool symbolize(const ModuleInfo& module, std::uint32_t rva,
                   ResolvedFrame& out) override {
        if (!ensureSymbols()) {
            return false;
        }

        std::lock_guard<std::mutex> lock(g_symbolMutex);
        const DWORD64 address = module.base + rva;
        constexpr DWORD kNameChars = 512;

        std::vector<unsigned char> storage(sizeof(SYMBOL_INFOW) +
                                           kNameChars * sizeof(WCHAR));
        auto* symbol = reinterpret_cast<SYMBOL_INFOW*>(storage.data());
        symbol->SizeOfStruct = sizeof(SYMBOL_INFOW);
        symbol->MaxNameLen = kNameChars;

        DWORD64 displacement = 0;
        if (!SymFromAddrW(GetCurrentProcess(), address, &displacement, symbol)) {
            return false;
        }

        out.function =
            utf8FromWide(std::wstring(symbol->Name, symbol->NameLen));

        IMAGEHLP_LINEW64 line{};
        line.SizeOfStruct = sizeof(line);
        DWORD lineDisplacement = 0;
        if (SymGetLineFromAddrW64(GetCurrentProcess(), address, &lineDisplacement,
                                  &line)) {
            out.file = utf8FromWide(line.FileName != nullptr ? line.FileName : L"");
            out.line = line.LineNumber;
        }
        return !out.function.empty();
    }

    std::uint64_t currentThreadId() const override {
        return GetCurrentThreadId();
    }

    void waitForInterval(std::chrono::nanoseconds interval,
                         const std::atomic<bool>& stop) override {
        if (stop.load(std::memory_order_acquire)) {
            return;
        }
        if (m_timer == nullptr || m_wakeEvent == nullptr) {
            std::this_thread::sleep_for(interval);
            return;
        }

        const long long nanoseconds =
            std::max<long long>(interval.count(), 100);
        LARGE_INTEGER due{};
        due.QuadPart = -(nanoseconds / 100);

        SetWaitableTimer(m_timer, &due, 0, nullptr, nullptr, FALSE);
        const HANDLE handles[2] = {m_timer, m_wakeEvent};
        WaitForMultipleObjects(2, handles, FALSE, INFINITE);
    }

    void wake() override {
        if (m_wakeEvent != nullptr) {
            SetEvent(m_wakeEvent);
        }
    }

private:
    struct ModuleRecord {
        ModuleInfo info;
        std::vector<std::pair<std::uint64_t, std::uint64_t>> execRanges;
    };

    static bool stackLimits(HANDLE thread, std::uint64_t& low,
                            std::uint64_t& high) {
        ThreadBasicInformationData information{};
        const NTSTATUS status = NtQueryInformationThread(
            thread, static_cast<THREADINFOCLASS>(0), &information,
            sizeof(information), nullptr);
        if (status < 0 || information.TebBaseAddress == nullptr) {
            return false;
        }

        const auto* tib = reinterpret_cast<const NT_TIB*>(
            information.TebBaseAddress);
        low = reinterpret_cast<std::uint64_t>(tib->StackLimit);
        high = reinterpret_cast<std::uint64_t>(tib->StackBase);
        return low != 0 && high != 0 && low < high;
    }

    bool mapAddress(std::uint64_t address, bool requireExecutable,
                    std::uint32_t& moduleIndex, std::uint32_t& rva) const {
        for (std::size_t i = 0; i < m_modules.size(); ++i) {
            const ModuleRecord& record = m_modules[i];
            const std::uint64_t base = record.info.base;
            if (address < base || address >= base + record.info.size) {
                continue;
            }
            if (requireExecutable) {
                bool executable = false;
                for (const auto& range : record.execRanges) {
                    if (address >= range.first && address < range.second) {
                        executable = true;
                        break;
                    }
                }
                if (!executable) {
                    return false;
                }
            }
            moduleIndex = static_cast<std::uint32_t>(i);
            rva = static_cast<std::uint32_t>(address - base);
            return true;
        }
        return false;
    }

    static void readImageMetadata(ModuleRecord& record) {
        const auto base = reinterpret_cast<const std::uint8_t*>(
            static_cast<std::uintptr_t>(record.info.base));
        if (base == nullptr) {
            return;
        }

        const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(base);
        if (dos->e_magic != IMAGE_DOS_SIGNATURE) {
            return;
        }

        const auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS64*>(
            base + dos->e_lfanew);
        if (nt->Signature != IMAGE_NT_SIGNATURE ||
            nt->OptionalHeader.Magic != IMAGE_NT_OPTIONAL_HDR64_MAGIC) {
            return;
        }

        const IMAGE_SECTION_HEADER* section = IMAGE_FIRST_SECTION(nt);
        for (WORD i = 0; i < nt->FileHeader.NumberOfSections; ++i, ++section) {
            if ((section->Characteristics & IMAGE_SCN_MEM_EXECUTE) == 0) {
                continue;
            }
            const std::uint64_t begin =
                record.info.base + section->VirtualAddress;
            std::uint64_t size = section->Misc.VirtualSize;
            if (size == 0) {
                size = section->SizeOfRawData;
            }
            if (size != 0) {
                record.execRanges.emplace_back(begin, begin + size);
            }
        }

        const IMAGE_DATA_DIRECTORY& debugDirectory =
            nt->OptionalHeader
                .DataDirectory[IMAGE_DIRECTORY_ENTRY_DEBUG];
        if (debugDirectory.VirtualAddress == 0 ||
            debugDirectory.Size < sizeof(IMAGE_DEBUG_DIRECTORY)) {
            return;
        }

        const auto* debug = reinterpret_cast<const IMAGE_DEBUG_DIRECTORY*>(
            base + debugDirectory.VirtualAddress);
        const std::size_t debugCount =
            debugDirectory.Size / sizeof(IMAGE_DEBUG_DIRECTORY);

        for (std::size_t i = 0; i < debugCount; ++i) {
            if (debug[i].Type != IMAGE_DEBUG_TYPE_CODEVIEW ||
                debug[i].SizeOfData < 24) {
                continue;
            }
            const auto* codeView = reinterpret_cast<const std::uint8_t*>(
                base + debug[i].AddressOfRawData);
            if (std::memcmp(codeView, "RSDS", 4) != 0) {
                continue;
            }
            std::memcpy(record.info.buildId, codeView + 4, 16);
            std::memcpy(record.info.buildId + 16, codeView + 20, 4);
            record.info.buildIdSize = 20;
            break;
        }
    }

    bool ensureSymbols() {
        std::lock_guard<std::mutex> lock(g_symbolMutex);

        const HANDLE process = GetCurrentProcess();
        if (!g_symbolsInitialized) {
            SymSetOptions(SYMOPT_UNDNAME | SYMOPT_DEFERRED_LOADS |
                          SYMOPT_LOAD_LINES | SYMOPT_FAIL_CRITICAL_ERRORS |
                          SYMOPT_NO_PROMPTS);

            std::wstring searchPath;
            auto appendPath = [&searchPath](const std::wstring& path) {
                if (path.empty()) {
                    return;
                }
                if (!searchPath.empty()) {
                    searchPath += L';';
                }
                searchPath += path;
            };

            for (const ModuleRecord& record : m_modules) {
                appendPath(directoryOf(wideFromUtf8(record.info.path)));
            }

            wchar_t environment[4096]{};
            const DWORD length =
                GetEnvironmentVariableW(L"_NT_SYMBOL_PATH", environment,
                                        static_cast<DWORD>(std::size(environment)));
            if (length > 0 && length < std::size(environment)) {
                appendPath(environment);
            }

            if (!SymInitializeW(process,
                                searchPath.empty() ? nullptr : searchPath.c_str(),
                                FALSE)) {
                if (GetLastError() != ERROR_ALREADY_INITIALIZED) {
                    return false;
                }
            }
            g_symbolsInitialized = true;
        }

        for (const ModuleRecord& record : m_modules) {
            if (record.info.base == 0 || record.info.path.empty()) {
                continue;
            }
            const std::wstring path = wideFromUtf8(record.info.path);
            SymLoadModuleExW(process, nullptr, path.c_str(), nullptr,
                             record.info.base,
                             static_cast<DWORD>(record.info.size), nullptr, 0);
        }
        return true;
    }

    std::vector<ModuleRecord> m_modules;
    HANDLE m_timer = nullptr;
    HANDLE m_wakeEvent = nullptr;
};

std::unique_ptr<IProfilerBackend> createPlatformBackend() {
    return std::make_unique<WindowsBackend>();
}

} // namespace ve::profiler
