# Vosta Engine

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE.txt)

A modern C++20 game engine with deferred PBR rendering, a node-based material
editor, ECS architecture, and physically-based atmosphere with volumetric
clouds.

## Features

- **Deferred renderer** — GBuffer, HDR buffer, post-processing, and TAA passes
- **Physically-based atmosphere** — CPU-baked transmittance / multi-scatter
  LUTs, planet-centered sky rendering
- **Volumetric clouds** — Worley noise baked on CPU, half-res fragment raymarch,
  weather-map driven coverage and density
- **IBL-based PBR** — precomputed specular + diffuse irradiance (IBL baker)
- **Node-based material editor** — visual graph UI with runtime shader
  generation
- **ECS scene architecture** — entities, components, systems, and binary scene
  serialization (`.veworld`)
- **Terrain support**
- **Dear ImGui editor interface** with viewport gizmos (ImGuizmo) and file
  browser

## Requirements

- CMake 3.21+
- A C++20 compiler (MSVC 2022+, Clang 16+, GCC 13+)
- OpenGL 4.3+
- Windows (primary, pre-built GLFW is vendored); Linux / macOS use the system
  GLFW package

## Build

```bash
cmake -B build
cmake --build build --config Release
```

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
    Renderer/           Render pipeline, PBR, atmosphere, clouds, material graph
    Scene/              ECS, entities, serialization
    Core/               Application, logging, resource manager
    Platform/           GLFW windowing + OpenGL backend
    Asset/              Model / texture import
  thirdparty/           Vendored dependencies
VostaEditor/            Editor application (ImGui panels, material graph UI)
SandBox/                Demo / testbed application
docs/                   Design notes
```

## License

Vosta Engine is released under the [MIT License](LICENSE.txt).

Third-party libraries used by the project are listed with their licenses in
[THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md).
