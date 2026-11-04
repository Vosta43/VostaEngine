#pragma once

#include "Core.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace ve {
namespace vfs {

// Read facade over the loose filesystem and any mounted VEGPAK archives.
//
// Every entry point takes an ABSOLUTE path (what toAbsolute /
// toProjectAbsolute produce). It is mapped back to a package-root-relative key
// -- relative to the project root first, then the asset root -- and looked up
// in the mounted archives before the filesystem. Archives are indexed
// case-insensitively because packages are produced on Windows, where a loose
// path that differs only in case still resolves.
//
// Callers that must keep loose-file semantics bit-identical (text-mode reads,
// incremental IO) gate on isArchived() and keep their existing stream path;
// everyone else may call readFile unconditionally, which falls back to the
// filesystem.

// Mount an archive. Call after the roots are set and before any loading.
VE_API bool mountPak(const std::string& absolutePath, std::string* error = nullptr);
VE_API void unmountAll();
VE_API bool isMounted();

// True when the path resolves to a key stored in a mounted archive.
VE_API bool isArchived(const std::string& absolutePath);

VE_API bool exists(const std::string& absolutePath);
VE_API bool fileSize(const std::string& absolutePath, uint64_t& size);

// Whole-file read. The archive wins over a loose file with the same key.
VE_API bool readFile(const std::string& absolutePath, std::vector<uint8_t>& out,
                     std::string* error = nullptr);

// Raw bytes as a string; no newline translation. `out` is left empty on failure.
VE_API bool readText(const std::string& absolutePath, std::string& out,
                     std::string* error = nullptr);

// Reads up to `size` bytes from the start. `read` is the count actually
// available (a short file is a valid prefix); returns false only when the file
// cannot be opened.
VE_API bool readPrefix(const std::string& absolutePath, void* dst, size_t size,
                       size_t& read);

// One immediate child of a directory. Directories exist in an archive only
// implicitly, so they are reconstructed from the stored file keys at mount.
struct DirEntry {
    std::string name;
    bool        directory = false;
};

// Lists the immediate children of `absoluteDir`, archive entries first and then
// loose files the archive does not shadow. Case-insensitive, sorted by name.
// Returns false when the directory exists in neither source.
VE_API bool listDirectory(const std::string& absoluteDir, std::vector<DirEntry>& out);

} // namespace vfs
} // namespace ve
