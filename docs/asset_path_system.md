# Asset Path System

## Overview

All asset paths in Vosta Engine are stored as **project-relative paths** (relative to the project root directory). At I/O boundaries, paths are resolved to absolute filesystem paths via a single, globally-configured asset root. This eliminates the previous chaos of CWD-relative paths, cross-directory `../` references, and ad-hoc walk-up resolution functions.

```
Project Root (e.g. D:/CORE/Project/Vosta Engine/)
├── SandBox/          ← SandBox.exe working directory
│   └── assets/
│       ├── shaders/
│       ├── textures/
│       ├── models/
│       └── scenes/
├── VostaEngine/      ← Engine DLL
│   ├── resources/
│   │   ├── icons/
│   │   └── fonts/
│   └── src/
└── VostaEditor/      ← Editor EXE working directory
```

---

## Core API — `AssetConfig.h`

| Function | Purpose |
|----------|---------|
| `setAssetRoot(absolutePath)` | Called once at startup. Stores the absolute project root. Asserts if called twice. |
| `getAssetRoot()` | Returns the stored absolute root path. |
| `toAbsolute(path)` | If `path` is absolute → return normalized. If relative → prepend asset root. |
| `toRelative(path)` | Strips asset root prefix. If path is outside root → warn + return unchanged. |

All four functions are **free functions** in the `ve` namespace, exported from `VostaEngine.dll` via `VE_API`.

### Examples

```cpp
// Assume asset root = "D:/CORE/Project/Vosta Engine"

toAbsolute("SandBox/assets/textures/ve.png")
// → "D:/CORE/Project/Vosta Engine/SandBox/assets/textures/ve.png"

toRelative("D:/CORE/Project/Vosta Engine/SandBox/assets/models/sphere.obj")
// → "SandBox/assets/models/sphere.obj"

toAbsolute("C:/Users/me/Downloads/model.obj")   // already absolute
// → "C:/Users/me/Downloads/model.obj"           (returned as-is)

toRelative("C:/Users/me/Downloads/model.obj")    // outside asset root
// → warns, returns "C:/Users/me/Downloads/model.obj" unchanged
```

---

## Initialization Flow

Both entry points (`SandBox/EntryPoint.cpp` and `VostaEditor/src/VostaEditor.cpp`) follow the same pattern:

```
1. Walk up from current_path() until a directory containing
   both "VostaEngine/" and "SandBox/" is found → project root.

2. Call ve::setAssetRoot(projectRoot).

3. Create Application → layers → everything else.
```

The walk-up strategy means the executables can be launched from any working directory and still locate the project root, as long as the folder structure is intact.

---

## Path Storage — `ResourceManager`

`ResourceManager` is the central registry. Every asset loaded from disk passes through it.

```cpp
template<typename T>
static AssetHandle store(const std::string& path) {
    std::string relativePath = toRelative(path);   // normalize to relative
    std::string absolutePath = toAbsolute(relativePath); // resolve for I/O
    Ref<T> resource = T::create(absolutePath);     // factory uses absolute path
    if (!resource) return INVALID_ASSET_HANDLE;
    return storage<T>().store(relativePath, resource); // stored by relative key
}
```

**Key invariant**: All keys in `ResourceStorage<T>` are project-relative paths. `getPath(handle)` returns a relative path. `find(path)` and `contains(path)` normalize their argument via `toRelative()` before lookup.

### Why the double conversion?

`toRelative` first: even if the caller passes an absolute path, we normalize to relative for the storage key. Then `toAbsolute` resolves it back to absolute for the factory function (`T::create`), which always receives an absolute path for file I/O.

---

## Factory Functions — `T::create()`

Each resource factory calls `toAbsolute()` on its input path before opening files:

| Factory | File | What it does |
|---------|------|--------------|
| `Texture2D::create(path)` | `Renderer/Texture.cpp` | `stbi_load(toAbsolute(path))` |
| `TextureCubeMap::create(path)` | `Renderer/Texture.cpp` | Reads 6 face images via absolute path |
| `StaticMesh::create(path)` | `Renderer/StaticMesh.cpp` | Assimp `ReadFile(toAbsolute(path))` |
| `Shader::create(path)` | `Renderer/Shader.cpp` | `ifstream` on `toAbsolute(path)` |
| `Material::create(path)` | `Renderer/Material.cpp` | Loads `.veasset`, stores `name = toRelative(path)` |

Factories receive an already-absolute path from `ResourceManager::store()`, but also call `toAbsolute()` themselves as a safety net (defense-in-depth for direct calls that bypass ResourceManager).

---

## DLL Boundary — `VE_API`

`VE_API` is defined in `Core.h`:

```cpp
#ifdef VE_BUILD_DLL
    #define VE_API __declspec(dllexport)   // building the DLL
#else
    #define VE_API __declspec(dllimport)   // consuming the DLL
#endif
```

- `VostaEngine.vcxproj` (Release x64) defines `VE_BUILD_DLL` → exports `setAssetRoot`, `toAbsolute`, `toRelative`, `getAssetRoot`.
- `VostaEditor.vcxproj` and `SandBox.vcxproj` do NOT define it → they import these symbols from `VostaEngine.dll`.

The `getAssetRoot()` return type (`const std::filesystem::path&`) is safe across the DLL boundary because all modules share the same CRT and `std::filesystem::path` implementation.

---

## Typical Call Chain

```
User code:
  ResourceManager::store<Texture2D>("SandBox/assets/textures/ve.png")
    │
    ├─ toRelative("SandBox/assets/textures/ve.png")
    │     → "SandBox/assets/textures/ve.png" (already relative)
    │
    ├─ toAbsolute("SandBox/assets/textures/ve.png")
    │     → "D:/.../SandBox/assets/textures/ve.png"
    │
    ├─ Texture2D::create("D:/.../SandBox/assets/textures/ve.png")
    │     └─ toAbsolute(path) → already absolute, returns normalized
    │     └─ stbi_load(resolvedPath) → pixel data
    │
    └─ storage<Texture2D>().store("SandBox/assets/textures/ve.png", resource)
          → key = "SandBox/assets/textures/ve.png" (relative)
```

---

## Serialization

`SceneSerializer` and `Material` serialization use `ResourceManager::getPath<T>(handle)` which returns the **relative** path string. This path is written directly into `.veworld` / `.veasset` files. On deserialization, the path flows back through `ResourceManager::store()` which normalizes it again. No changes were needed to the serialization layer.

---

## Edge Cases

| Scenario | Behavior |
|----------|----------|
| Path already absolute | `toAbsolute` returns it normalized; `toRelative` strips root if under it, warns + returns unchanged if not |
| Path outside asset root | `toRelative` warns and returns as-is; TODO: copy-on-import |
| `setAssetRoot` called twice | Asserts in debug, no-op in release (second call ignored) |
| Empty path | Both functions return `""` immediately |
| ShaderLibrary paths | ShaderLibrary does NOT go through ResourceManager; paths are resolved at each call site via `toAbsolute()` |
