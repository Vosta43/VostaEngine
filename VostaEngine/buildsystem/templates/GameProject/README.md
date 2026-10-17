# {{PROJECT_NAME}}

A Vosta Engine game project. The engine is vendored in `engine/`, so this folder
is self-contained -- move it anywhere and it still builds and runs.

## Layout

```
{{PROJECT_NAME}}.slnx      Solution: the game module only
{{PROJECT_NAME}}.vcxproj   Gameplay module -> Binaries/game.dll
src/Game.cpp               Your gameplay code (start here)
engine/                    The engine: include/, lib/, bin/, buildsystem/, resources/, docs/
content/                   Your project content (scenes, imported assets)
Binaries/                  Build output (game.dll); created on build
```

## Run it

1. Open `{{PROJECT_NAME}}.slnx` in Visual Studio 2022.
2. Press F5.

That builds `Binaries/game.dll` and launches `engine/bin/VostaPlayer.exe` on this
folder.

## Where to write game code

Everything lives in `src/Game.cpp` -- one class, `Game`:

- `onStart` -- runs once when a play session begins, with a throwaway runtime
  `Scene` you may mutate freely. Build your world here.
- `onUpdate` -- runs every frame while the session is live.
- `onEnd` -- runs once when the session stops.
- `onLoad` / `onUnload` -- optional module-wide setup (e.g. registering component
  types). Leave them out unless you need them.

`onUpdate` is driven by the runtime scene's own tick, so a host that pauses the
scene (the editor's Play session) pauses the game with it.

Add more `.cpp` files next to `src/Game.cpp` and list them in the `.vcxproj`
under the existing `ClCompile` item.

## Updating the engine

`engine/` is a copy, pinned to the version this project was created with. To move
to a newer engine, replace `engine/` with the payload from a newer SDK build --
this is an explicit upgrade, and the risk of any behavior change is yours. Nothing
updates `engine/` automatically; a project stays on the version it shipped with
until you choose to re-copy.
