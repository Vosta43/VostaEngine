#include "vepch.h"
#include "Platform/Process.h"

#include "Core/Log.h"

#include <fstream>
#include <iterator>

namespace ve {

namespace {

    // Quotes one argument for the MSVCRT command-line parser -- the inverse of
    // the rules CreateProcess-fed programs (cl.exe, msbuild) follow. A run of N
    // backslashes before a quote must be doubled so the quote is not consumed as
    // an escape; a plain run is passed through.
    void appendQuoted(std::string& out, const std::string& arg) {
        const bool needsQuotes = arg.empty() ||
            arg.find_first_of(" \t\n\v\"") != std::string::npos;
        if (!needsQuotes) {
            out += arg;
            return;
        }

        out.push_back('"');
        size_t backslashes = 0;
        for (const char c : arg) {
            if (c == '\\') {
                ++backslashes;
                continue;
            }
            if (c == '"') {
                out.append(backslashes * 2 + 1, '\\');
                out.push_back('"');
            } else {
                out.append(backslashes, '\\');
                out.push_back(c);
            }
            backslashes = 0;
        }
        // Trailing backslashes would escape the closing quote.
        out.append(backslashes * 2, '\\');
        out.push_back('"');
    }

    std::string buildCommandLine(const std::vector<std::string>& argv) {
        std::string line;
        for (size_t i = 0; i < argv.size(); ++i) {
            if (i) line.push_back(' ');
            appendQuoted(line, argv[i]);
        }
        return line;
    }

} // namespace

ProcessResult runProcess(const std::vector<std::string>& argv, const std::string& processDir,
                         bool wait) {
    ProcessResult result;
    if (argv.empty()) return result;

    // The child writes into a scratch file rather than a pipe: a pipe needs a
    // drain thread to keep a chatty child from blocking once the buffer fills,
    // while a file has no such deadlock and is read back in one go afterwards.
    char tempDir[MAX_PATH] = {};
    char tempPath[MAX_PATH] = {};
    if (GetTempPathA(MAX_PATH, tempDir) == 0 ||
        GetTempFileNameA(tempDir, "vst", 0, tempPath) == 0) {
        VE_CORE_ERROR_PRINT("%s", "runProcess: could not create a scratch log");
        return result;
    }

    SECURITY_ATTRIBUTES sa{};
    sa.nLength        = sizeof(sa);
    sa.bInheritHandle = TRUE;

    HANDLE log = CreateFileA(tempPath, GENERIC_WRITE | GENERIC_READ, FILE_SHARE_READ,
                             &sa, CREATE_ALWAYS, FILE_ATTRIBUTE_TEMPORARY, nullptr);
    if (log == INVALID_HANDLE_VALUE) {
        DeleteFileA(tempPath);
        VE_CORE_ERROR_PRINT("%s", "runProcess: could not open the scratch log");
        return result;
    }

    STARTUPINFOA si{};
    si.cb         = sizeof(si);
    si.dwFlags    = STARTF_USESTDHANDLES;
    si.hStdOutput = log;
    si.hStdError  = log;
    si.hStdInput  = GetStdHandle(STD_INPUT_HANDLE);

    PROCESS_INFORMATION pi{};

    // CreateProcess may write into the command line buffer, so it needs a mutable
    // copy rather than the built string's storage.
    std::string line = buildCommandLine(argv);
    std::vector<char> cmd(line.begin(), line.end());
    cmd.push_back('\0');
    const char* dir = processDir.empty() ? nullptr : processDir.c_str();

    const BOOL started = CreateProcessA(nullptr, cmd.data(), nullptr, nullptr, TRUE,
                                        CREATE_NO_WINDOW, nullptr, dir, &si, &pi);
    if (!started) {
        VE_CORE_ERROR_PRINT("runProcess: CreateProcess failed (%lu)", GetLastError());
        CloseHandle(log);
        DeleteFileA(tempPath);
        return result;
    }

    if (!wait) {
        // The child outlives this call; dropping our handles does not end it.
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
        CloseHandle(log);
        DeleteFileA(tempPath);
        result.started = true;
        return result;
    }

    WaitForSingleObject(pi.hProcess, INFINITE);
    DWORD code = 1;
    GetExitCodeProcess(pi.hProcess, &code);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    CloseHandle(log);

    result.started  = true;
    result.exitCode = static_cast<int>(code);

    std::ifstream in(tempPath, std::ios::binary);
    if (in)
        result.output.assign(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
    DeleteFileA(tempPath);

    return result;
}

} // namespace ve
