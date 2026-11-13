# Vosta Engine

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE.txt)
![Version](https://img.shields.io/badge/version-0.2.15-blue.svg)

A modern C++20 game engine built around deferred PBR rendering, a data-driven
render pipeline, node-based material, noise and geometry graphs, quad-tree
terrain, and a physically-based atmosphere with volumetric clouds. Games are
written as C++ gameplay modules loaded at runtime, with server-authoritative
multiplayer and a multi-world Play-in-Editor. It ships with a Dear ImGui editor
whose scene can be scripted by AI agents over MCP.

## Features

### Rendering

- **Declarative render pipeline** — passes declared in JSON, with declarative
  input bindings, per-pass `settings` / `when` conditions, viewport modes,
  partial render state, and `draw` blocks that select geometry sources by
  render layer and per-draw uniform source
- **Deferred renderer** — GBuffer, HDR lighting, post-processing, and TAA passes
- **Draw sources** — full-screen, static meshes, sprites, globe tiles, skybox
  and particles, each driven by the same pass descriptor
- **Render layers** — a bitmask authored per renderer, matched by a pass's
  `draw` block
- **Shadow mapping** — cascaded directional shadows
- **IBL-based PBR** — precomputed specular prefilter and diffuse irradiance (IBL baker)
- **Physically-based atmosphere** — CPU-baked transmittance / multi-scatter LUTs,
  planet-centered sky rendering
- **Volumetric clouds** — CPU-baked Worley noise, half-res fragment raymarch,
  weather-map driven coverage and density
- **Runtime virtual textures** — paged RVT for terrain layer blending and the
  planet globe
- **Planet-scale geometry** — cube-sphere tiles that place the flat terrain
  chart onto a sphere
- **Space background** — procedural cube-sphere star field with per-star
  blackbody tint, composited behind the atmosphere

### Scene & assets

- **ECS architecture** — type-erased component registry, entity duplicate/delete,
  and an editor "Add Component" UI
- **JSON scene serialization** (`.veworld`)
- **Prefab registry** with built-in prefabs
- **Project model** — an engine root plus a switchable project root (`project.json`)
- **Asset library and authoring**, with static meshes (OBJ, and FBX via ufbx),
  textures, fonts, sounds, materials and layered materials
- **Play-in-Editor** — the authored scene is cloned once per player, and each
  clone ticks in its own world (see *Gameplay*); the authored scene is never
  mutated

### Gameplay

- **C++ gameplay modules** — a project builds `Binaries/game.dll`; the host
  verifies its module API version and drives `onStart` / `onUpdate` / `onEnd`.
  A project template and a scriptable SDK assembly ship with the engine.
- **Server-authoritative networking** — replicated snapshots at a fixed 20 Hz
  over an ENet transport, with fields opted in through reflection
  (`VEPROPERTY(..., "net=replicated")`), client-side interpolation, and typed
  RPCs. The editor's Play session runs a listen server plus clients (2–4
  players) in one process.
- **Physics** — Jolt rigid bodies stepped with the scene in play mode, with
  transforms written back to the ECS
- **Audio** — a miniaudio mixer with sfx / music / UI buses, voices, a 3D
  listener, and WAV / FLAC / MP3 / OGG decoding
- **HUD** — asset-defined widget trees drawn through Vellum, ticked by a scene
  system and bound from C++ by name

### Materials

- **Node-based material editor** — a visual graph with runtime shader generation
- **Layered materials** — standalone `MaterialLayerAsset`s blended per layer
- **Material instances** — single- and layered-material variants

### Terrain

- **Quad-tree terrain** with a mesh builder and terrain-data resources
- **Material layer painting** — world-space, seamless across tiles
- **Sculpting tools** — raise / lower / smooth / flatten / sharpen / erosion brushes

### Noise

- **Noise graph** — node-based procedural noise with a CPU evaluator

### Geometry

- **Geometry graph** — node-based procedural meshes (icosphere, subdivide,
  noise displacement, field displacement, SDF booleans and marching cubes)
  evaluated to a mesh asset
- **Cube-sphere builders** — LOD-aware tiles for planet-scale rendering

### Particles

- **Module-graph emitter runtime** — four-stage emitters (emitter spawn/update,
  particle spawn/update) built from modules and compiled to a small register VM
- **Particle assets** (`.veasset`) — module stack, parameters, and attribute
  layout saved as text, recompiled on load
- **Billboard rendering** — camera-facing quads with per-particle colour, plus a
  dedicated editor panel with a live viewport

### UI

- **Vellum** — a self-contained retained-mode UI library (layout, input,
  painting, design-size scaling) used by the HUD and available to gameplay
- **Animation** — curve sequences on an absolute clock that drive render
  transforms and opacity without touching layout or hit-testing
- **CJK text** — Noto Sans SC fallback with a shared glyph atlas

### Packaging

- **Self-contained exports** — the packager assembles the player, the engine
  runtime and the project's content into a versioned output folder
- **PAK archives** — content ships as loose files or as a `VEGPAK` archive,
  optionally encrypted with XChaCha20-Poly1305 chunk sealing

### Diagnostics

- **CPU sampling profiler** — F11 capture with function/self-time resolution
  and speedscope export

### AI / MCP

- **MCP tool layer** — scene, asset, and editor tools behind a command registry
  and a threaded dispatch queue
- **In-editor AI panel** — an LLM-driven agent that edits the scene through the
  same tool layer
- **Agent knowledge base** — engine-owned, version-locked docs under
  `VostaEngine/resources/mcp/`

### Editor

- Dear ImGui interface with ImGuizmo viewport gizmos
- Node-graph editor (imgui-node-editor) for materials, noise, geometry, terrain
  rules and layered materials
- Panels for the scene outliner, properties, file browser, asset manager, JSON
  editor, material / noise / geometry graphs, particles, terrain map, texture
  preview, and AI
- Multi-world Play-in-Editor with tiled viewports and a player-count control
- CJK-capable UI font (Noto Sans SC)

## Requirements

- CMake 3.21+
- A C++20 compiler (Visual Studio 2026 / MSVC v145, Clang 16+, GCC 13+)
- OpenGL 4.6+
- Windows is the primary target (pre-built GLFW is vendored); Linux and macOS use
  the system GLFW package

## Build

```bash
cmake -B build
cmake --build build --config Release
```

On Windows you can also open `VostaEngine/VostaEngine.slnx` directly in Visual
Studio 2026.

The following CMake options are available:

| Option             | Description                 | Default |
| ------------------ | --------------------------- | ------- |
| `VE_BUILD_EDITOR`  | Build the VostaEditor app   | ON      |
| `VE_BUILD_PLAYER`  | Build the VostaPlayer host  | ON      |
| `VE_BUILD_SANDBOX` | Build the SandBox demo app  | ON      |

## Run

```bash
./build/VostaEditor/Release/VostaEditor    # editor
./build/VostaPlayer/Release/VostaPlayer    # standalone runtime host
```

## Running a project

`VostaPlayer` is the standalone runtime host. It opens a project directory,
renders that project's default scene, and loads `<project>/Binaries/game.dll` as
its gameplay module when present:

```bash
VostaPlayer path/to/MyProject      # defaults to SandBox/ when omitted
```

The engine must locate its own resources (`VostaEngine/resources/`). It resolves
the **engine root** from, in order:

1. **`VOSTA_ENGINE_ROOT`** — environment variable pointing at the directory that
   contains `VostaEngine/resources/`. This is how the dev tree runs: the build
   output layout is not a shipping layout, so the root cannot be derived from the
   binary alone. Visual Studio passes it through `LocalDebuggerEnvironment`, so
   F5 works without any manual setup; a shell launch sets it explicitly.
2. **The executable's location** — the directory holding the binary, then its
   parent. A packaged build keeps the player beside the resource tree; an SDK
   build keeps it in `bin/`, one level below the root.

The working directory is never consulted, so a project can sit anywhere on disk.

## Creating a project

New projects are self-contained: a project folder carries its own copy of the
engine in `engine/`, so it builds and runs anywhere with no configuration.

```text
MyGame/
  MyGame.slnx          solution: the gameplay module only
  MyGame.vcxproj       builds Binaries/game.dll
  src/Game.cpp         your gameplay code
  engine/              a copy of the engine (include/, lib/, bin/, buildsystem/, resources/)
  content/             project content
```

To create one:

1. Build the **`VostaEngineSdk`** project in `VostaEngine/VostaEngine.slnx`. It
   assembles `SDK/` — the engine payload (`include/`, `lib/`, `bin/`,
   `buildsystem/`, `VostaEngine/resources/`) plus the project template
   (`templates/GameProject/`). `SDK/` is a build product and is gitignored.
2. In the editor, create a new project and enable the gameplay-module option.
   The generator copies the template and materializes the engine payload into
   `<project>/engine/`.
3. Open `<project>/<name>.slnx` in Visual Studio and press F5.

Upgrading a project's engine is an explicit re-copy of `SDK/` into its `engine/`
folder. Nothing updates it automatically, so a project stays on the engine
version it was created with.

## Project Structure

```
CMakeLists.txt          Top-level build (engine, SandBox, editor, player)
VostaEngine/            Core engine library
  src/
    Core/               Application, window, layers, resources, project roots,
                        gameplay modules, VFS
      Events/           Event dispatcher
      Diagnostics/      CPU sampling profiler
    Network/            ENet transport, replication, RPCs, snapshot interpolation
    Renderer/           Render pipeline, PBR, atmosphere, clouds, RVT
      Pipeline/         Data-driven pass configuration
      RenderPass/       Draw, HDR, TAA, cloud, shadow, present, RVT passes
      Shadow/           Cascaded shadow setup
      Preprocess/       Atmosphere sky and IBL bakers
    Scene/              ECS, prefabs, serialization, scene cloning
      ECSystems/        Scene tick systems (particles, HUD, audio, physics)
      Terrain/          Quad-tree terrain
    Terrain/            Planet rule graph and globe weight baking
    Geometry/           Geometry graph evaluation, cube-sphere meshes
    Sdf/                SDF value and marching-cubes nodes
    Noise/              Node-based procedural noise
    Graph/              Shared node-graph core
    Particle/           Module-graph emitters and billboarding
    Asset/              Asset library, mesh / texture import
    Audio/              miniaudio device, decoding, sound assets
    Physics/            Jolt world wrapper
    Build/              Packager (loose, PAK, encrypted PAK)
    Gui/                Vellum UI library and HUD system
    MCP/                MCP tool layer, LLM client, and agent session
    Math/               Shared math
    Platform/           GLFW windowing + OpenGL backend
  buildsystem/          SDK assembly and the shared game-module props file
  resources/            Shaders, pipelines, fonts, built-in meshes, agent docs
  tests/                ProfilerSmoke and NetSmoke harnesses
  thirdparty/           Vendored dependencies
VostaEditor/            Editor application (panels, viewport, graph editors)
VostaPlayer/            Standalone runtime host
SandBox/                Built-in default project
Projects/               Per-machine project roots (gitignored)
SDK/                    Packaged engine + project template (build product, gitignored)
```

## License

Vosta Engine is released under the [MIT License](LICENSE.txt).

Third-party libraries used by the project are listed with their licenses in
[THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md).
