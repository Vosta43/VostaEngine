# Vosta Engine — agent knowledge base

**Engine version: `0.2.6-dev`** (last release `0.2.5`). These docs describe the
on-disk formats of this specific build.

> **Version check — do this first.** If the engine you are driving reports a
> version other than the one above, **stop and ask the user whether to continue**,
> and tell them the risk: a mismatched build may have changed tokens, field
> layouts or node tags. Silently acting on stale structure can write corrupt
> `.veasset` files. Only proceed if the user accepts that risk.

Machine-facing docs for AI agents that drive the engine through its MCP server.

These files describe the **engine**, not any particular project, so they are
engine-owned and version-locked: they live next to the engine DLL and change
with it. Knowledge about a specific game (its scenes, art, gameplay) belongs in
that project's own folder, not here.

**Read this index first, then only the one file you need.** Loading every file
wastes context; each one is written to answer a specific class of question.

| File | Read it when you need to… |
|---|---|
| `engine_asset.md` | …resolve or load an asset, understand the path model, or work with `.veasset` files |
| `scenes.md` | *(planned)* …read or edit scenes, entities and components |
| `rendering.md` | *(planned)* …touch the render pipeline, materials or shaders |
| `terrain.md` | *(planned)* …edit quad-tree terrain |

## How to use these docs

- **They are a map, not a spec.** The authoritative schema for every format
  lives in code (`Scene/Archive.h`, `Core/Json.h`, the importer sources). If a
  doc and the code disagree, **the code wins** — and the doc should be fixed.
- **They explain *why* and *where*, not *what*.** Anything derivable by reading
  the source is deliberately left out, because a copy would rot. Expect pointers
  to files and symbols rather than exhaustive field lists.
- **Practical over complete.** A terse rule that stops you making a mistake beats
  an exhaustive description you have to wade through.
