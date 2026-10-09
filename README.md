# Vosta Engine

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE.txt)
![Version](https://img.shields.io/badge/version-0.2.6-blue.svg)

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
- OpenGL 4.3+
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
| `VE_BUILD_SANDBOX` | Build the SandBox demo app  | ON      |

## Run

```bash
./build/VostaEditor/Release/VostaEditor    # editor
./build/SandBox/Release/SandBox            # demo / testbed
```

## Project Structure

```
VostaEngine/            Core engine library
  src/
    Renderer/           Render pipeline, PBR, atmosphere, clouds, shadows
      Pipeline/         Data-driven pass configuration
      RenderPass/       GBuffer, HDR, TAA, cloud, shadow, present passes
      Shadow/           Cascaded shadow setup
    Scene/              ECS, prefabs, serialization, quad-tree terrain
    Noise/              Node-based procedural noise
    Core/               Application, logging, resources, project roots
    Platform/           GLFW windowing + OpenGL backend
    Asset/              Asset library, mesh / texture import
    MCP/                Model Context Protocol tool layer
    AI/                 LLM client and agent session
    Gui/                Shared ImGui widgets
  resources/            Icons, fonts, agent docs
  thirdparty/           Vendored dependencies
VostaEditor/            Editor application (ImGui panels, graph and terrain editors)
SandBox/                Demo / testbed application
Projects/               Per-machine project roots (gitignored)
docs/                   Design notes
```

## License

Vosta Engine is released under the [MIT License](LICENSE.txt).

Third-party libraries used by the project are listed with their licenses in
[THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md).
