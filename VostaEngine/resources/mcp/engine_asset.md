# Vosta Engine — asset formats

Every engine asset on disk, its type token, and its exact serialized layout.

**Scope:** engine-owned asset *formats*. Scene files, the render pipeline and
materials-in-a-scene are covered elsewhere (`scenes.md`, `rendering.md`).

> The layouts below are positional — fields are read back in the written order,
> with no names. A single missing or extra field desynchronises the whole file
> past that point. When in doubt, the authority is the `serialize`/`deserialize`
> pair in the source (cited per format).

---

## 1. The one thing to know first

**`.veasset` is a single extension covering many asset kinds.** The kind is not
in the extension — it is a **type token** stored as the first item in the file.
There are also **two different encodings** behind the same extension:

| Encoding | Used by | On disk |
|---|---|---|
| `TextArchive` | `material`, `layered_material`, `material_layer`, `noise` | ASCII, whitespace-delimited, human-readable |
| `BinaryArchive` | `texture`, `staticmesh` | opaque host-endian byte blobs |

`utils::isEngineAsset(path)` is literally just "extension == `.veasset`"
(`Asset/Utils.h`).

---

## 2. Identifying a `.veasset` without loading it

`utils::peekAssetToken(path)` (`Asset/Utils.h`) reads only the leading token,
which is enough to route a file to the right loader. It works by inspecting
**byte 0**:

- **byte 0 in `[0x20, 0x7F)`** → ASCII digit → **TextArchive**. Layout is
  `"<len> <token>"` (decimal length, one space, raw token bytes).
- **otherwise** → **BinaryArchive**. Layout is `<uint32 little-endian len>` +
  raw token bytes.
- Token length must be `1..64` or the peek fails and returns `""`.

Both encodings length-prefix the token, which is what makes the trick work.

---

## 3. Path model (background)

Two independent roots (`Core/AssetConfig.h`):

- **Asset root** — engine install dir. `setAssetRoot` (once-only) +
  `toAbsolute` / `toRelative`. **All `ResourceManager` keys are asset-root-relative.**
- **Project root** — the open project's folder. `setProjectRoot` (switchable) +
  `toProjectAbsolute` / `toProjectRelative`.

A path stored *inside* an asset file (e.g. a texture reference) is
asset-root-relative. Projects live under the asset root
(`<assetRoot>/Projects/<name>/`), so project paths appear as
`Projects/<name>/content/...` relative keys. The built-in default project is
`SandBox`, i.e. `SandBox/content/...`.

---

## 4. Catalog — TextArchive formats

Shared primitives (`Scene/Archive.h`, `TextArchive`):

- `int32` / `float` → decimal text + one trailing space. Floats use default
  ostream precision (~6 significant digits) — **lossy on round-trip**.
- `string` → `<len> <raw bytes> ` (length-prefixed, so spaces inside are safe).
- `vec2/3/4` → their floats in sequence. **There is no `mat4` overload.**
- `writeBytes`/`readBytes` are **no-ops** — text assets cannot carry raw bytes.

### 4.1 Embedded node-graph encoding

Only `material` and `noise` bodies contain a graph. Both use the same layout
(`Graph/NodeGraph.cpp`), after their own header:

```
int32  nodeCount
  repeat nodeCount:
    uint8   tag            # from the kind's *NodeTag enum; written via int32
    int32   id
    vec2    m_pos          # float x, float y
    string  m_displayName
    int32   m_nodeType      # NodeType enum: Input/Math/Vector/Texture/Output/Utility
    <node-specific extras>  # see per-kind tables
int32  linkCount
  repeat linkCount:
    int32 id, startPin.id, startPin.pinIndex, endPin.id, endPin.pinIndex
```

Gotchas:

- Next-id counters are **not stored**; they are recomputed as `maxId+1`.
- A node whose `tag` is not in the registry is **silently skipped** on read —
  counts still parse, but links referencing a dropped node become dangling.

### 4.2 `material` — single PBR material

Writer: `Renderer/SingleMaterial.cpp` (`serialize`). Token: `material`.

```
string name                    # asset path; NOT trusted — re-stamped on load
string albedoPath, normalPath, metallicPath, roughnessPath, aoPath, emissivePath
vec3   albedoColor
float  metallic, roughness, ao
vec3   emissiveColor
int32  isCompiled
<node graph>                    # §4.1
```

Material node tags (`MaterialNodeTag`, `Renderer/MaterialNodes.h`; registry
`MaterialGraph.cpp`):

| tag | node | extra params after base |
|---|---|---|
| 0 | ConstantFloat | `float value` |
| 1 | Constant2Vector | `vec2 value` |
| 2 | Constant3Vector | `vec3 value` |
| 3 | Constant4Vector | `vec4 value` |
| 4 | TextureCoordinate | `vec2 uvScale` |
| 5 | TextureSampler | `string texturePath` |
| 6 | Multiply | — |
| 7 | Add | — |
| 8 | Subtract | — |
| 9 | Divide | — |
| 10 | Lerp | — |
| 11 | Clamp | — |
| 12 | MaterialOutput | — |

`TextureSamplerNode.samplerName` is **never serialized** — it is regenerated at
load. A new on-disk material starts as one `MaterialOutputNode`.

### 4.3 `layered_material`

Writer: `Renderer/LayeredMaterial.cpp`. Token: `layered_material`.

```
int32  version        # must be 2 (kFormatVersion) — mismatch discards the body
string name
float  heightScale, macroStrength, macroScale
int32  layerCount     # clamped to 0..4 (kMaxMaterialLayers)
  repeat layerCount:
    string layerAssetPath   # path to a material_layer asset; "" = invalid handle
```

### 4.4 `material_layer`

Writer: `Renderer/MaterialLayerAsset.cpp`. Token: `material_layer`.

```
int32  version        # must be 2 — mismatch discards the body
string name
string albedoPath, normalPath, roughnessPath, heightPath
float  tiling
int32  useSlope
float  slopeMin, slopeMax
int32  useHeight
float  heightMin, heightMax
float  noiseStrength, noiseScale
int32  blend          # enum: 0=Weight, 1=Alpha, 2=Height
```

`blend` is written **last**, after the noise pair — the easiest field to misplace.

### 4.5 `noise` — noise graph

Writer: the editor's NoisePanel (mirrors this layout). Reader:
`Noise/NoiseGraphResource.cpp`. Token: `noise`.

```
string noise
int32  version        # must be 2 — mismatch discards the file
<node graph>          # §4.1
```

Noise node tags (`NoiseNodeTag`, `Noise/NoiseNodes.h`; registry
`NoiseGraph.cpp`):

| tag | node | extra params after base |
|---|---|---|
| 0 | NoiseUnit | full `NoiseSettings` (§ below) |
| 1 | Add | — |
| 2 | Subtract | — |
| 3 | Multiply | — |
| 4 | Divide | — |
| 5 | Output | `float outputMin, outputMax` |
| 6 | Constant | `float value` |
| 7 | Blend | — |
| 8 | Min | — |
| 9 | Max | — |
| 10 | Invert | — |
| 11 | Clamp | `float minValue, maxValue` |
| 12 | Remap | `float inMin, inMax, outMin, outMax; int32 clamp` |
| 13 | Threshold | `float threshold, falloff` |
| 14 | Abs | — |
| 15 | Erf | `float divisor` |
| 16 | Trunc | — |
| 17 | Spline | `uint32 pointCount`, then `pointCount` × `vec2` |

`NoiseSettings` (`Noise/NoiseSettings.h`), 16 fields in order — the payload of
every `NoiseUnit` node:

```
int32 type             # OpenSimplex2, OpenSimplex2S, Cellular, Perlin, ValueCubic, Value
int32 seed
float frequency
int32 fractal          # None, Fbm, Ridged, PingPong
int32 octaves
float lacunarity
float gain
float weightedStrength
float pingPongStrength
int32 cellularDistance
int32 cellularReturn
float cellularJitter
int32 domainWarp       # None, OpenSimplex2, OpenSimplex2Reduced, BasicGrid
float domainWarpAmp
float outputMin
float outputMax
```

Enum orders mirror FastNoiseLite so the backend mapping is a plain cast.

---

## 5. Catalog — BinaryArchive formats

Shared primitives (`Scene/Archive.cpp`, `BinaryArchive`): everything is raw
host-endian `memcpy` — no padding markers, no versioning header. `string` is
`<uint32 len>` + raw bytes (no terminator). `writeBytes`/`readBytes` copy N raw
bytes. Any read past the buffer sets `isGood() == false`.

> **Portability caveat:** because these files are raw struct memory in host
> endianness, they are **not portable across platforms or compilers**. Treat a
> `.veasset` binary as belonging to the build that wrote it.

### 5.1 `texture` — baked texture

Writer/reader: `Asset/TextureImporter.cpp`. Token: `texture`. Self-contained —
decoded pixels are baked in; loading never needs the source image.

```
string texture
int32  width, height, format   # TextureFormat: NONE=0,RGB=1,RGBA=2,R8=3,
                               #   RG16F=4,RGB16F=5,RGBA16F=6,R16F=7
int32  pixelBytes              # LDR: byte count
<raw>  pixelBytes              # LDR pixel data
int32  floatCount              # HDR: float element count
<raw>  floatCount * 4 bytes    # HDR float data
```

An LDR source fills `pixels` (the float section is 0); an HDR source (`.hdr`)
fills `floatPixels` (the byte section is 0). The reader checks the token
(`"Not a texture asset (token=...)"`) and rejects truncation, but does **not**
sanity-check width/height.

### 5.2 `staticmesh` — baked mesh

Writer/reader: `Asset/StaticMeshImporter.cpp`. Token: `staticmesh`.

```
string staticmesh
int32  vertexCount, indexCount, submeshCount
<raw>  vertexCount * sizeof(Vertex)
<raw>  indexCount  * sizeof(int)
  repeat submeshCount:
    string name
    int32  firstIndex, indexCount
    string materialPath     # "" = unassigned (INVALID_ASSET_HANDLE)
```

`Vertex` is `glm::vec3 position; glm::vec2 uv; glm::vec3 normal; glm::vec3 tangent;`
= 44 bytes, written as **raw struct memory** (host layout). Material handles
travel as asset paths and are re-stored through `ResourceManager` on load.
Negative counts are rejected on read.

### 5.3 Cube map — no on-disk format

`TextureCubeMapImporter::serialize` / `deserialize` are **empty stubs** in
`Asset/TextureImporter.cpp` ("reserve for offline processing pipeline"). A cube
map exists only in memory today; there is no `.veasset` layout for it yet.

---

## 6. Non-`.veasset` assets (pointers)

| Extension | What | Where |
|---|---|---|
| `.veworld` | Scene document, **JSON** (`SceneSerializer` uses `JsonWriter`/`JsonReader`) | see `scenes.md` |
| `.json` | `project.json` (project manifest: `name`, `defaultScene`); render-pipeline configs under `assets/pipelines/` | see `rendering.md` |
| `.glsl` | Shader source, loaded as text | `rendering.md` |
| `.png` `.jpg` `.hdr` `.obj` `.mtl` | **External source** files, not engine assets — imported and baked into `.veasset` | §7 |
| `<scene>.veworld.camera.json` | Editor viewport state, a sidecar kept out of the scene so moving the camera never dirties the document | — |

`utils::isEngineAsset` returns **false** for all of these — only `.veasset`
counts as an engine asset.

Note: older scene files in a pre-migration token format (the same
length-prefixed style the text archives use) may still exist on disk. New scenes
are JSON. If a `.veworld` does not start with `{`, it predates the migration.

---

## 7. Loading and identity

`ResourceManager` (`Core/ResourceManager.h`) is the single entry point:

- `store<T>(path)` — normalises to asset-relative, returns an existing handle if
  already loaded (**dedup**), else `T::create(toAbsolute(path))` and stores under
  the relative key. `INVALID_ASSET_HANDLE` on failure.
- `get<T>(handle)`, `getPath<T>(handle)`, `find<T>(path)`, `contains<T>`,
  `remove<T>`, `forEach<T>`.
- `AssetHandle` (`Core/AssetHandle.h`) is `{index, generation}` — a **stale
  handle** fails `get` instead of returning a recycled resource. `isValid()`
  is `index != 0xFFFFFFFF`.
- Template bodies live in `ResourceManager.cpp` with explicit instantiation, so
  exactly **one** storage instance exists, in the engine DLL.

**Built-in assets** (`Asset/BuiltinReousrces.h`) live in memory under reserved
`__builtin_...` keys, never as files — so they never collide with project
content and never leave stray `.veasset`s: `getDefaultMaterial` (white matte,
fallback), `getDefaultWhiteTexture` (1×1, fallback for un-overridden texture
params), `getBuiltinSphere` (generated UV sphere; **needs a live GL context**).

---

## 8. Importing external files

`Asset/ImportManager.cpp` — `import(sourcePath, destDir)`:

- Copies the source **and everything it references** (`.obj` → `.mtl` →
  textures) into `destDir` first, so the project stays self-contained.
- Then bakes the mesh to `<stem>.veasset` next to it.
- Textures are baked by `TextureImporter::serialize` into a binary `.veasset`.

Source files outside the asset root trigger a `toRelative` warning and are
returned unchanged — the import step is what brings them under the root.

---

## 9. Gotchas, collected

- `.veasset` ≠ one format. Check the **token**, and remember two encodings.
- Text floats lose precision beyond ~6 significant digits.
- Text assets physically cannot hold binary data (`writeBytes` is a no-op).
- Binary assets are host-endian raw struct memory — not cross-platform.
- Version mismatches **discard** the file body rather than misreading it.
- Unknown node tags are dropped silently; links can end up dangling.
- A material's stored `name`/path is stale by design — re-stamped from the real
  path on load. Never trust it when writing.
