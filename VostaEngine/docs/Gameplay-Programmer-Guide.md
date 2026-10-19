# Gameplay Programming Guide

How to write a game module in C++ against Vosta Engine. This is task-oriented:
it is meant to get you from nothing to spawning, moving, and reacting to things
in a scene. It is not an exhaustive reference — for the full surface, the headers
are the reference. The *API map* at the end says which header to open for what.

Applies to engine **0.2.9** (`VE_GAME_MODULE_API_VERSION` **2**).

## 1. The model in one minute

- A **game module** is a DLL (`game.dll`) built from your project. The engine
  **host** loads it and drives it. There is no `main` of your own — the host owns
  the loop.
- The module is one class deriving `ve::GameModule`. The host calls it through a
  small virtual interface; the vtable order is the ABI, and the version is
  checked at load.
- Two hosts run that same DLL through the same path:
  - **VostaPlayer** — `engine/bin/VostaPlayer.exe <projectDir>`. Loads the
    project's default scene and plays it. (F5 in your project does this.)
  - **The editor's Play session (PIE)** — duplicates the scene you authored and
    plays the duplicate in-viewport.
- Your code talks to a `ve::Scene`: an entity/component store. There is no
  `GameObject` class. An entity is a `uint32_t` id, and behaviour is data in
  components.

## 2. Your project and how it builds

A project is self-contained: the engine is vendored in `engine/`, so the folder
can be moved anywhere and still builds and runs.

```
<Project>/
  <Project>.slnx        Solution: the game module only
  <Project>.vcxproj     Gameplay module -> Binaries/game.dll
  src/Game.cpp          Your code (start here)
  engine/               The pinned engine: include/, lib/, bin/, buildsystem/, resources/
  content/              Your content (scenes, imported assets)
  Binaries/             Output: game.dll (created on build)
```

`<Project>.vcxproj` imports `engine/buildsystem/Vosta.GameModule.props`, which
supplies every include path, the import lib (`engine/lib/VostaEngine.lib`), the
output name (`game.dll` into `Binaries/`), and the F5 target
(`engine/bin/VostaPlayer.exe .`). Your `.vcxproj` only lists your own sources:

```xml
<ItemGroup>
  <ClCompile Include="src\Game.cpp" />
  <ClCompile Include="src\Enemy.cpp" />
</ItemGroup>
```

Include the umbrella and you have the whole public surface:

```cpp
#include <VostaEngine.h>
```

## 3. The module lifecycle

Derive `ve::GameModule`, override the hooks you need, and export it with
`VE_GAME_MODULE`:

```cpp
#include <VostaEngine.h>

class Game : public ve::GameModule {
public:
    // Once, when the DLL loads. The GL context is already live here.
    void onLoad(ve::Application& app) override {}

    // Play session. `scene` is a throwaway runtime world -- mutate it freely.
    void onStart(ve::Scene& scene) override {
        // Build your world here.
    }

    // Every frame while the session is live.
    void onUpdate(ve::Scene& scene, float deltaTime) override {
        // Per-frame gameplay.
    }

    void onEnd(ve::Scene& scene) override {}

    // Once, before the DLL is freed.
    void onUnload(ve::Application& app) override {}
};

VE_GAME_MODULE(Game)
```

Hook order and timing:

| Hook | When | Notes |
|------|------|-------|
| `onLoad(Application&)` | DLL load, once | GL context live. Register globals. |
| `onStart(Scene&)` | session start, once | `scene` is a duplicate of the authored scene. |
| `onUpdate(Scene&, float)` | every frame | `deltaTime` in seconds. |
| `onEnd(Scene&)` | session stop, once | runs before the scene is torn down. |
| `onUnload(Application&)` | DLL unload, once | release module state. |

The host does not call you on a timer of its own. It registers a per-frame tick
**on the scene**, so `onUpdate` runs from `Scene::onUpdate(deltaTime)`. Both hosts
tick the scene, so your update behaves the same in the player and in PIE — and if
a host pauses the scene (PIE while stopped), your game pauses with it. Do not try
to run your own loop.

`VE_GAME_MODULE(Game)` emits the C-ABI entry points the host resolves with
`GetProcAddress`. One module per DLL.

## 4. Scene and ECS

`ve::Scene` owns an `EntityRegistry`. An entity is just an id; components are
plain structs stored per-type. Iterate the live ids:

```cpp
for (uint32_t id : scene.getRegistry().each()) {
    // id is a live entity id
}
```

Read a component — guard with `has` first, because `get` on a missing component
returns a shared placeholder rather than failing:

```cpp
ve::EntityRegistry& reg = scene.getRegistry();
if (reg.has<ve::NameComponent>(id))
    const std::string& name = reg.get<ve::NameComponent>(id).name;
```

`Scene` wraps the common cases: `getComponent<T>(e)`,
`assignComponent<T>(e, args...)`, `eachComponent<T>()`, `createEntity()`,
`destroyEntity(e)`, `duplicateEntity(e)`.

Create an entity and give it components:

```cpp
ve::Entity e = scene.createEntity();
scene.assignComponent<ve::NameComponent>(e, std::string("Crate"));
scene.assignComponent<ve::TransformComponent>(
    e, glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.0f, 5.0f)));
```

Find an entity by name — there is no name index, so scan:

```cpp
ve::Entity findByName(ve::Scene& scene, const std::string& want) {
    auto& reg = scene.getRegistry();
    for (uint32_t id : reg.each())
        if (reg.has<ve::NameComponent>(id) &&
            reg.get<ve::NameComponent>(id).name == want)
            return scene.getEntity(id);
    return {};   // id 0xFFFFFFFF is the invalid handle
}
```

### Transform is a matrix, not TRS

`ve::TransformComponent` holds a single `glm::mat4 transform`, with translation
in the last column — there is no `position`/`rotation`/`scale` split and no
`setPosition`. To spin an entity about its own vertical axis, right-multiply a
rotation so the translation is left untouched:

```cpp
const float radiansPerSecond = 0.9f;
const glm::mat4 spin = glm::rotate(glm::mat4(1.0f),
                                   radiansPerSecond * deltaTime,
                                   glm::vec3(0.0f, 1.0f, 0.0f));
scene.getComponent<ve::TransformComponent>(e).transform *= spin;
```

To place or move, compose onto the matrix (`glm::translate`, `glm::scale`, an
orbit is a left-multiply about the world origin).

### Built-in components

From `Scene/Components.h`: `NameComponent`, `TransformComponent`,
`CameraComponent`, `LightComponent`, `StaticMeshComponent`, `SkyBoxComponent`,
`SpriteRendererComponent`, `AtmosphereComponent`, `TerrainSystemComponent`,
`TerrainComponent`.

### Custom components

Define your own with three macros. It self-registers with the engine, so the
editor's *Add Component* menu lists it and it serializes into the scene file:

```cpp
VESTRUCT(HealthComponent)
struct HealthComponent {
    VEPROPERTY(HealthComponent, float, hp, "HP",
               "type=drag,minValue=0,maxValue=100")
    float hp = 100.0f;

    void serialize(ve::JsonWriter& w) const { w.set("hp", hp); }
    void deserialize(const ve::JsonReader& r) { hp = r.getFloat("hp", hp); }
};
VECOMPONENT(HealthComponent, "Health", "Gameplay")
```

- `VESTRUCT` goes before the struct body, `VECOMPONENT` after it (the factory it
  generates needs a complete type).
- `VEPROPERTY` also drives the inspector widget through its `meta` string
  (`type=drag|input|bool`, `minValue`, `maxValue`, `speed`, ...).
- Optional: `VECOMPONENTINIT(T, { ... })` runs once after the component is added
  or deserialized — the place to acquire the resources it needs.

Use it like any built-in component:

```cpp
scene.assignComponent<HealthComponent>(e);
scene.getComponent<HealthComponent>(e).hp -= 10.0f * deltaTime;
```

## 5. Recipes

### Time

```cpp
const float dt = ve::DeltaTime::get().getDeltaTime();   // seconds
```

### Keyboard and mouse

```cpp
if (ve::Input::isKeyPressed(VE_KEY_W))               { /* ... */ }
if (ve::Input::isMousePressed(VE_MOUSE_BUTTON_LEFT)) { /* ... */ }
auto [mx, my] = ve::Input::getMousePosition();
```

Key and mouse constants live in `KeyCodes.h` / `MouseButtonCodes.h`.

### Putting a mesh in the world

Static meshes are referenced by `ve::AssetHandle`. The handle persists as a path
string and is re-resolved on load, so a scene file references assets by path.
Usually the editor has already imported what you need; to resolve a path
yourself:

```cpp
ve::AssetHandle mesh =
    ve::ResourceManager::store<ve::StaticMesh>("content/Crate.veasset");
if (mesh.isValid())
    scene.assignComponent<ve::StaticMeshComponent>(e, mesh);
```

### Logging

```cpp
VE_CORE_INFO_PRINT("spawned %d entities", count);   // printf-style
VE_CORE_SUCCESS_PRINT("loaded '%s'", path.c_str());
VE_CORE_WARN_PRINT("missing asset '%s'", path.c_str());
VE_CORE_ERROR_PRINT("bad entity id %u", id);
```

These take a printf format string. There are also stream-style macros
(`VE_CORE_ERROR`, `VE_CORE_SUCCESS`) that build the message with `<<` — do not
hand those a `%s` format string, it will not be substituted.

### A Vellum UI panel

Vellum is the engine's retained-mode UI. A layer hands the engine one root
widget tree; the engine samples input and runs the tree for it during the UI
phase. Fetch the engine layer, build a tree, and set it as the root:

```cpp
if (auto* ui = ve::Application::get().getVellumLayer()) {
    ve::vellum::FontAtlas* font = ui->font(20.0f);

    auto panel = ve::CreateRef<ve::vellum::Stack>();
    panel->add(ve::CreateRef<ve::vellum::ColoredBox>(
        glm::vec4(0.10f, 0.11f, 0.13f, 0.95f), glm::vec2(400.0f, 200.0f)));
    panel->add(ve::CreateRef<ve::vellum::TextBlock>(font, "Hello"),
               glm::vec2(24.0f, 24.0f));

    auto button = ve::CreateRef<ve::vellum::Button>(
        glm::vec2(200.0f, 40.0f), glm::vec4(0.2f, 0.5f, 0.8f, 1.0f));
    button->setOnClick([] { /* ... */ });
    panel->add(button, glm::vec2(24.0f, 80.0f));

    auto root = ve::CreateRef<ve::vellum::Stack>();
    root->add(panel, glm::vec2(0.0f), glm::vec2(0.5f));   // centred
    ui->setRoot(root);
}
```

Two rules that bite (details in *Pitfalls*): change a tree by rebuilding it and
calling `setRoot` again, and never call `setRoot` from inside a widget callback.

## 6. The API map

`<VostaEngine.h>` pulls in the public surface. By subsystem, the headers to open:

| Subsystem | Header(s) | What you get |
|-----------|-----------|--------------|
| App & lifecycle | `Core/Application.h`, `Core/Layers/Layers.h`, `Core/GameModule.h`, `Core/GameModuleHost.h` | `Application`, `Layer`, the module interface, the host |
| Time | `Core/Deltatime.h` | `DeltaTime::get()` |
| Logging | `Core/Log.h` | the `VE_CORE_*` macros |
| Reflection | `Core/Reflection.h` | `VESTRUCT` / `VEPROPERTY` and inspector meta |
| Events | `Core/Events/*.h` | `EventDispatcher` and window/mouse events |
| Windowing | `Core/Window.h` | the `Window` handle |
| Jobs | `Core/JobSystem.h` | the engine job system |
| Scene & ECS | `Scene/Scene.h`, `Scene/EntityRegistry.h`, `Scene/Components.h`, `Scene/Entity.h` | entities, components, the registry |
| Serialization | `Scene/SceneSerializer.h`, `Scene/PrefabRegistry.h` | load/save worlds, prefabs |
| Input | `Input.h`, `KeyCodes.h`, `MouseButtonCodes.h` | keyboard and mouse |
| Assets | `Core/ResourceManager.h`, `Core/AssetHandle.h`, `Asset/ImportManager.h`, `Asset/StaticMeshResource.h` | asset handles and importing |
| Mesh & material | `Renderer/StaticMesh.h`, `Renderer/Material.h` | mesh and material types |
| Rendering | `Renderer/Camera.h`, `Renderer/SceneView.h`, `Renderer/RenderPipeline.h`, `Renderer/SceneRenderer.h`, `Renderer/Renderer*.h`, `Renderer/Shader.h`, `Renderer/FrameBuffer.h` | cameras, the view/pipeline, low-level commands |
| UI | `Gui/Gui.h`, `Gui/Vellum/*` | ImGui (`Gui`) and the Vellum widgets |
| MCP | `MCP/*.h` | tool/command registry, LLM client, agent session |

Some areas are reachable by a direct `#include` (the folder `engine/include/src`
is on your include path) but are not part of the umbrella's supported surface —
for example `Platform/*`, `Build/*`, and the deeper terrain/noise internals. Treat
those as engine-internal.

## 7. Pitfalls

- **Transform is a matrix, not TRS.** No `setPosition`; compose on the
  `glm::mat4`. To spin in place, right-multiply (see *Scene and ECS*).
- **`onUpdate` is scene-driven, not a wall clock.** It runs from
  `Scene::onUpdate`. A host that pauses the scene pauses you.
- **`get<T>` never fails loudly.** A missing component yields a per-type
  placeholder. Check `has<T>` first whenever absent is possible.
- **Vellum has no remove-child.** Change a tree by rebuilding it and calling
  `setRoot` again — rebuilding is cheap.
- **Never call `setRoot` from inside a widget callback.** The input router holds
  a raw pointer to the widget it is delivering to, and `setRoot` invalidates it —
  a dereference crash. Mark your state dirty and rebuild in `onUpdate` instead.
- **Vellum's default font is Latin-only.** Non-Latin (e.g. CJK) text renders
  blank until the engine's default UI font is changed.
- **No GL objects before the context.** Do not create GL resources during static
  init; the context is live only once layers attach / a session starts.
- **Mismatched module version.** A `game.dll` built against a different
  `VE_GAME_MODULE_API_VERSION` is refused by the host. Rebuild the module when you
  upgrade `engine/`.

## 8. Updating the engine

`engine/` is a copy pinned to the version the project was created with. Upgrade
by replacing it with the payload from a newer SDK build — an explicit action, and
the risk of any behaviour change is yours. Nothing updates `engine/`
automatically. Because your module is compiled against
`VE_GAME_MODULE_API_VERSION`, an upgrade that bumped it also requires rebuilding
your `game.dll`; the host will refuse a stale one rather than run it.
