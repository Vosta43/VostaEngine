#include "vepch.h"
#include "Platform/Process.h"

#include "Core/Log.h"

#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

#include <sys/wait.h>
#include <unistd.h>

namespace ve {

namespace {

    // Deliberately hand-rolled: after fork() the child may only call
    // async-signal-safe functions, so neither printf nor strerror is allowed --
    // both can deadlock on a lock another thread held when fork ran.
    [[noreturn]] void childFail(const char* what, int code) {
        char message[160];
        size_t length = 0;

        auto append = [&](const char* text) {
            while (*text && length + 1 < sizeof(message))
                message[length++] = *text++;
        };

        append("runProcess: ");
        append(what);
        append(" failed (errno ");

        char digits[12];
        size_t digitCount = 0;
        unsigned value = code < 0 ? 0u : static_cast<unsigned>(code);
        do {
            digits[digitCount++] = static_cast<char>('0' + value % 10);
            value /= 10;
        } while (value);
        while (digitCount && length + 1 < sizeof(message))
            message[length++] = digits[--digitCount];
        append(")\n");

        (void)::write(STDERR_FILENO, message, length);
        ::_exit(127);
    }

    // Runs in the forked child, where only async-signal-safe calls are legal:
    // no allocation, no locks, no engine logging. A failure is written to stderr
    // (already the scratch file) and the child leaves without returning.
    [[noreturn]] void becomeChild(const char* program, char* const argv[],
                                  const char* processDir, int logFd) {
        // Redirect first so that even a chdir failure is captured in the log.
        if (::dup2(logFd, STDOUT_FILENO) < 0)
            childFail("dup2(stdout)", errno);
        if (::dup2(logFd, STDERR_FILENO) < 0)
            childFail("dup2(stderr)", errno);
        if (logFd != STDOUT_FILENO && logFd != STDERR_FILENO)
            ::close(logFd);

        if (processDir && *processDir) {
            if (::chdir(processDir) != 0)
                childFail("chdir", errno);
        }

        ::execvp(program, argv);
        childFail("execvp", errno);
    }

} // namespace

ProcessResult runProcess(const std::vector<std::string>& argv, const std::string& processDir,
                         bool wait) {
    ProcessResult result;
    if (argv.empty()) return result;

    std::error_code ec;
    const std::filesystem::path tempDir = std::filesystem::temp_directory_path(ec);
    if (ec) {
        VE_CORE_ERROR_PRINT("runProcess: no temporary directory (%s)", ec.message().c_str());
        return result;
    }

    // mkstemp gives a unique descriptor that is already open, so the child only
    // has to dup2 it -- there is no path race between creation and the spawn.
    std::string pattern = (tempDir / "vosta-process-XXXXXX").string();
    std::vector<char> patternBuffer(pattern.begin(), pattern.end());
    patternBuffer.push_back('\0');

    const int logFd = ::mkstemp(patternBuffer.data());
    if (logFd < 0) {
        VE_CORE_ERROR_PRINT("runProcess: could not create a scratch log (%s)", std::strerror(errno));
        return result;
    }
    const std::string logPath(patternBuffer.data());

    // execvp wants a mutable, null-terminated argv; the strings themselves are
    // never written to.
    std::vector<char*> childArgv;
    childArgv.reserve(argv.size() + 1);
    for (const std::string& arg : argv)
        childArgv.push_back(const_cast<char*>(arg.c_str()));
    childArgv.push_back(nullptr);

    const char* dir = processDir.empty() ? nullptr : processDir.c_str();

    const pid_t pid = ::fork();
    if (pid < 0) {
        VE_CORE_ERROR_PRINT("runProcess: fork failed (%s)", std::strerror(errno));
        ::close(logFd);
        ::unlink(logPath.c_str());
        return result;
    }

    if (pid == 0) {
        if (!wait) {
            // Double fork: the middle process exits at once, so the caller never
            // has to reap the launched program and no zombie outlives the call.
            const pid_t detached = ::fork();
            if (detached < 0)
                ::_exit(127);
            if (detached == 0)
                becomeChild(childArgv[0], childArgv.data(), dir, logFd);
            ::_exit(0);
        }
        becomeChild(childArgv[0], childArgv.data(), dir, logFd);
    }

    ::close(logFd);

    int status = 0;
    pid_t reaped = -1;
    do {
        reaped = ::waitpid(pid, &status, 0);
    } while (reaped < 0 && errno == EINTR);

    result.started = reaped >= 0;
    if (wait && reaped >= 0) {
        // A launch failure (missing program, bad working directory) arrives as
        // exit code 127 with the reason in the captured output.
        result.exitCode = WIFEXITED(status) ? WEXITSTATUS(status) : -1;

        std::ifstream in(logPath, std::ios::binary);
        if (in)
            result.output.assign(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
    }

    ::unlink(logPath.c_str());

    if (reaped < 0)
        VE_CORE_ERROR_PRINT("runProcess: waitpid failed (%s)", std::strerror(errno));

    return result;
}

} // namespace ve
