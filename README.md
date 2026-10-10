# Vosta Engine

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE.txt)
![Version](https://img.shields.io/badge/version-0.2.7-blue.svg)

A modern C++20 game engine built around deferred PBR rendering, a data-driven
render pipeline, node-based material and noise graphs, quad-tree terrain, and a
physically-based atmosphere with volumetric clouds. It ships with a Dear ImGui
editor whose scene can be scripted by AI agents over MCP.

## Features

### Rendering

- **Data-driven render pipeline** — passes declared in JSON, with declarative
  input bindings and per-pass `settings` / `when` conditions
- **Deferred renderer** — GBuffer, HDR buffer, post-processing, and TAA passes
- **Shadow mapping** — cascaded directional shadows
- **IBL-based PBR** — precomputed specular prefilter and diffuse irradiance (IBL baker)
- **Physically-based atmosphere** — CPU-baked transmittance / multi-scatter LUTs,
  planet-centered sky rendering
- **Volumetric clouds** — CPU-baked Worley noise, half-res fragment raymarch,
  weather-map driven coverage and density

### Scene & assets

- **ECS architecture** — type-erased component registry, entity duplicate/delete,
  and an editor "Add Component" UI
- **JSON scene serialization** (`.veworld`)
- **Prefab registry** with built-in prefabs
- **Project model** — an engine root plus a switchable project root (`project.json`)
- **Asset library and authoring**, with static-mesh (OBJ, and FBX via ufbx) and
  texture import

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

### AI / MCP

- **MCP tool layer** — scene, asset, and editor tools behind a command registry
  and a threaded dispatch queue
- **In-editor AI panel** — an LLM-driven agent that edits the scene through the
  same tool layer
- **Agent knowledge base** — engine-owned, version-locked docs under
  `VostaEngine/resources/mcp/`

### Editor

- Dear ImGui interface with ImGuizmo viewport gizmos
- Node-graph editor (imgui-node-editor) for materials and noise
- Panels for the scene outliner, properties, file browser, material graph, noise,
  terrain map, texture preview, and AI
- CJK-capable UI font (Noto Sans SC)

## Requirements

- CMake 3.21+
- A C++20 compiler (MSVC 2022+, Clang 16+, GCC 13+)
- OpenGL 4.6+
- Windows is the primary target (pre-built GLFW is vendored); Linux and macOS use
  the system GLFW package

## Build

```bash
cmake -B build
cmake --build build --config Release
```

On Windows you can also open `VostaEngine/VostaEngine.slnx` directly in Visual
Studio 2022.

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
2. **The executable's location** — `<exeDir>/..`. This covers a self-contained
   install (the SDK), where the host binary sits one level below the root.

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
VostaEngine/            Core engine library
  src/
    Renderer/           Render pipeline, PBR, atmosphere, clouds, shadows
      Pipeline/         Data-driven pass configuration
      RenderPass/       GBuffer, HDR, TAA, cloud, shadow, present passes
      Shadow/           Cascaded shadow setup
      Preprocess/       Atmosphere sky and IBL bakers
    Scene/              ECS, prefabs, serialization, quad-tree terrain
    Noise/              Node-based procedural noise
    Core/               Application, logging, resources, project roots
    Platform/           GLFW windowing + OpenGL backend
    Asset/              Asset library, mesh / texture import
    MCP/                MCP tool layer, LLM client, and agent session
    Gui/                Shared ImGui widgets
  resources/            Shaders, pipelines, fonts, built-in meshes, agent docs
  thirdparty/           Vendored dependencies
VostaEditor/            Editor application (ImGui panels, graph and terrain editors)
VostaPlayer/            Standalone runtime host
SandBox/                Built-in default project
Projects/               Per-machine project roots (gitignored)
```

## License

Vosta Engine is released under the [MIT License](LICENSE.txt).

Third-party libraries used by the project are listed with their licenses in
[THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md).
