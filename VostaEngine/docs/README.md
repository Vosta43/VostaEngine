# Vosta Engine — Developer Docs

For programmers writing **gameplay** against the engine SDK. Read these together
with the engine copy vendored at `<project>/engine/` — the docs and the headers
ship as one version, so they stay in step.

## Contents

- [Gameplay-Programmer-Guide.md](Gameplay-Programmer-Guide.md) — the module
  lifecycle, the Scene/ECS API, copy-paste recipes, a per-subsystem map of what
  is public, and the pitfalls worth knowing before you hit them.

## Applies to

Engine **0.2.9** — `VE_GAME_MODULE_API_VERSION` **2**.

The module interface is versioned. A host refuses to load a `game.dll` built
against a different version, so if you upgrade `engine/`, rebuild the module too.

## The one rule

`<VostaEngine.h>` is the supported surface: if a type is reachable from it, it is
public and stable within the version. Everything else under `engine/include/src/`
is engine-internal and may change without notice — including headers you can
still `#include` because that folder is on your include path. When in doubt,
start from the umbrella header.
