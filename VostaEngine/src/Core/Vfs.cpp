#include "vepch.h"
#include "Core/Vfs.h"

#include "Asset/PakReader.h"
#include "Core/AssetConfig.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <unordered_map>

namespace ve {
namespace vfs {

namespace {

    struct Archive {
        Ref<pak::Reader> reader;
        std::string path;
        // Lowercased stored key -> stored key.
        std::unordered_map<std::string, std::string> index;
        // Lowercased parent key -> (lowercased child name -> entry). Folders are
        // implicit in an archive, so they are folded out of the file keys.
        std::unordered_map<std::string, std::unordered_map<std::string, DirEntry>> dirs;
    };

    std::mutex s_mutex;
    std::vector<Archive> s_archives;

    std::string lowerAscii(const std::string& value) {
        std::string out = value;
        for (char& c : out)
            c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        return out;
    }

    bool startsWithIgnoreCase(const std::string& text, const std::string& prefix) {
        if (text.size() < prefix.size()) return false;
        for (size_t i = 0; i < prefix.size(); ++i) {
            if (std::tolower(static_cast<unsigned char>(text[i])) !=
                std::tolower(static_cast<unsigned char>(prefix[i])))
                return false;
        }
        return true;
    }

    // The packager writes keys relative to the package root. In a package that
    // root is both the project root and the asset root; during development the
    // project lives under Projects/, so both roots are tried.
    bool toKey(const std::string& absolutePath, std::string& key) {
        const std::filesystem::path file =
            std::filesystem::path(absolutePath).lexically_normal();
        if (!file.is_absolute()) return false;
        const std::string path = file.generic_string();

        const std::filesystem::path roots[2] = { getProjectRoot(), getAssetRoot() };
        for (const std::filesystem::path& root : roots) {
            if (root.empty()) continue;
            std::string prefix = root.lexically_normal().generic_string();
            if (prefix.empty()) continue;
            if (prefix.back() != '/') prefix.push_back('/');
            if (path.size() <= prefix.size()) continue;
            if (!startsWithIgnoreCase(path, prefix)) continue;
            key = path.substr(prefix.size());
            return true;
        }
        return false;
    }

    // Like toKey, but also accepts a root folder itself (the empty key) so the
    // package root can be listed.
    bool toDirKey(const std::string& absolutePath, std::string& key) {
        const std::filesystem::path dir =
            std::filesystem::path(absolutePath).lexically_normal();
        if (!dir.is_absolute()) return false;
        const std::string path = dir.generic_string();

        const std::filesystem::path roots[2] = { getProjectRoot(), getAssetRoot() };
        for (const std::filesystem::path& root : roots) {
            if (root.empty()) continue;
            std::string prefix = root.lexically_normal().generic_string();
            if (prefix.empty()) continue;
            if (prefix.back() != '/') prefix.push_back('/');

            std::string rootPath = prefix;
            while (!rootPath.empty() && rootPath.back() == '/') rootPath.pop_back();
            if (path.size() == rootPath.size() && startsWithIgnoreCase(path, rootPath)) {
                key.clear();
                return true;
            }
            if (!startsWithIgnoreCase(path, prefix)) continue;

            key = path.substr(prefix.size());
            while (!key.empty() && key.back() == '/') key.pop_back();
            return true;
        }
        return false;
    }

    bool resolve(const std::string& absolutePath, Ref<pak::Reader>& reader,
                 std::string& stored) {
        std::string key;
        if (!toKey(absolutePath, key)) return false;
        const std::string lower = lowerAscii(key);

        std::lock_guard<std::mutex> lock(s_mutex);
        for (const Archive& archive : s_archives) {
            const auto it = archive.index.find(lower);
            if (it == archive.index.end()) continue;
            reader = archive.reader;  // Reader serialises its own IO
            stored = it->second;
            return true;
        }
        return false;
    }

    void setError(std::string* error, const std::string& message) {
        if (error) *error = message;
    }

} // namespace

bool mountPak(const std::string& absolutePath, std::string* error) {
    Ref<pak::Reader> reader = CreateRef<pak::Reader>();
    if (!reader->open(absolutePath, error)) return false;

    std::vector<std::string> files;
    if (!reader->list(files)) {
        setError(error, "cannot list " + absolutePath);
        return false;
    }

    Archive archive;
    archive.reader = std::move(reader);
    archive.path = absolutePath;
    archive.index.reserve(files.size());
    for (const std::string& file : files) {
        archive.index.emplace(lowerAscii(file), file);

        // Fold the file key into its parent folders so listDirectory is a lookup
        // rather than a scan of every key.
        std::string parent;
        size_t start = 0;
        while (true) {
            const size_t slash = file.find('/', start);
            if (slash == std::string::npos) break;
            const std::string name = file.substr(start, slash - start);
            if (!name.empty()) {
                archive.dirs[lowerAscii(parent)].emplace(lowerAscii(name),
                                                         DirEntry{ name, true });
                if (!parent.empty()) parent.push_back('/');
                parent.append(name);
            }
            start = slash + 1;
        }
        if (start < file.size()) {
            const std::string name = file.substr(start);
            archive.dirs[lowerAscii(parent)].emplace(lowerAscii(name),
                                                     DirEntry{ name, false });
        }
    }

    std::lock_guard<std::mutex> lock(s_mutex);
    s_archives.push_back(std::move(archive));
    return true;
}

void unmountAll() {
    std::lock_guard<std::mutex> lock(s_mutex);
    s_archives.clear();
}

bool isMounted() {
    std::lock_guard<std::mutex> lock(s_mutex);
    return !s_archives.empty();
}

bool isArchived(const std::string& absolutePath) {
    Ref<pak::Reader> reader;
    std::string stored;
    return resolve(absolutePath, reader, stored);
}

bool exists(const std::string& absolutePath) {
    if (isArchived(absolutePath)) return true;
    std::error_code ec;
    return std::filesystem::exists(absolutePath, ec);
}

bool fileSize(const std::string& absolutePath, uint64_t& size) {
    Ref<pak::Reader> reader;
    std::string stored;
    if (resolve(absolutePath, reader, stored))
        return reader->fileSize(stored, size);

    std::error_code ec;
    const std::uintmax_t bytes = std::filesystem::file_size(absolutePath, ec);
    if (ec) return false;
    size = static_cast<uint64_t>(bytes);
    return true;
}

bool readFile(const std::string& absolutePath, std::vector<uint8_t>& out,
              std::string* error) {
    Ref<pak::Reader> reader;
    std::string stored;
    if (resolve(absolutePath, reader, stored))
        return reader->readFile(stored, out, error);

    out.clear();
    std::ifstream in(absolutePath, std::ios::binary);
    if (!in) {
        setError(error, "cannot open " + absolutePath);
        return false;
    }
    in.seekg(0, std::ios::end);
    const std::streamoff end = in.tellg();
    if (end < 0) {
        setError(error, "cannot size " + absolutePath);
        return false;
    }
    in.seekg(0, std::ios::beg);

    out.resize(static_cast<size_t>(end));
    if (!out.empty()) {
        in.read(reinterpret_cast<char*>(out.data()),
                static_cast<std::streamsize>(out.size()));
        if (!in || static_cast<size_t>(in.gcount()) != out.size()) {
            out.clear();
            setError(error, "read failed: " + absolutePath);
            return false;
        }
    }
    return true;
}

bool readText(const std::string& absolutePath, std::string& out, std::string* error) {
    std::vector<uint8_t> bytes;
    if (!readFile(absolutePath, bytes, error)) {
        out.clear();
        return false;
    }
    out.assign(bytes.begin(), bytes.end());
    return true;
}

bool readPrefix(const std::string& absolutePath, void* dst, size_t size,
                size_t& read) {
    read = 0;
    if (!dst && size != 0) return false;

    Ref<pak::Reader> reader;
    std::string stored;
    if (resolve(absolutePath, reader, stored)) {
        uint64_t total = 0;
        if (!reader->fileSize(stored, total)) return false;
        const size_t count = static_cast<size_t>(std::min<uint64_t>(total, size));
        if (count != 0 && !reader->readAt(stored, 0, dst, count, nullptr))
            return false;
        read = count;
        return true;
    }

    std::ifstream in(absolutePath, std::ios::binary);
    if (!in) return false;
    in.read(static_cast<char*>(dst), static_cast<std::streamsize>(size));
    read = static_cast<size_t>(in.gcount());
    return true;
}

bool listDirectory(const std::string& absoluteDir, std::vector<DirEntry>& out) {
    out.clear();
    std::string key;
    if (!toDirKey(absoluteDir, key)) return false;
    const std::string lowerKey = lowerAscii(key);

    bool found = false;
    std::unordered_map<std::string, DirEntry> merged;  // lower name -> entry

    {
        std::lock_guard<std::mutex> lock(s_mutex);
        for (const Archive& archive : s_archives) {
            const auto dirIt = archive.dirs.find(lowerKey);
            if (dirIt == archive.dirs.end()) continue;
            found = true;
            // Archives are searched in mount order, so the first one wins.
            for (const auto& [lowerName, entry] : dirIt->second)
                merged.emplace(lowerName, entry);
        }
    }

    std::error_code ec;
    if (std::filesystem::is_directory(absoluteDir, ec)) {
        found = true;
        for (const auto& entry : std::filesystem::directory_iterator(absoluteDir, ec)) {
            const std::string name = entry.path().filename().string();
            const std::string lowerName = lowerAscii(name);
            if (merged.find(lowerName) != merged.end()) continue;  // archive shadows disk
            merged.emplace(lowerName, DirEntry{ name, entry.is_directory(ec) });
        }
    }
    if (!found) return false;

    out.reserve(merged.size());
    for (auto& [lowerName, entry] : merged)
        out.push_back(std::move(entry));
    std::sort(out.begin(), out.end(), [](const DirEntry& a, const DirEntry& b) {
        return lowerAscii(a.name) < lowerAscii(b.name);
    });
    return true;
}

} // namespace vfs
} // namespace ve
