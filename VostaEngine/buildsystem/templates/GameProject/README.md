# {{PROJECT_NAME}}

A Vosta Engine game project. The engine is vendored in `engine/`, so this folder
is self-contained -- move it anywhere and it still builds and runs.

## Layout

```
{{PROJECT_NAME}}.slnx      Solution: the game module only
{{PROJECT_NAME}}.vcxproj   Gameplay module -> Binaries/game.dll
src/Game.cpp               Your gameplay code (start here)
engine/                    The engine: include/, lib/, bin/, buildsystem/, resources/
content/                   Your project content (scenes, imported assets)
Binaries/                  Build output (game.dll); created on build
```

## Run it

1. Open `{{PROJECT_NAME}}.slnx` in Visual Studio 2022.
2. Press F5.

That builds `Binaries/game.dll` and launches `engine/bin/VostaPlayer.exe` on this
folder. The console prints `[{{PROJECT_NAME}}] gameplay layer attached` and
`onPlay, entities=...` once the scene is up.

## Where to write game code

Everything lives in `src/Game.cpp`:

- `GameLayer::onAttach` -- runs once, when the module is loaded.
- `GameModule::onLoad` -- register components/systems and push gameplay layers.
- `GameModule::onPlay` -- runs when a play session starts, with a throwaway
  runtime `Scene` you may mutate freely.

Add more `.cpp` files next to `src/Game.cpp` and list them in the `.vcxproj`
under the existing `ClCompile` item.

## Updating the engine

`engine/` is a copy, pinned to the version this project was created with. To move
to a newer engine, replace `engine/` with the payload from a newer SDK build --
this is an explicit upgrade, and the risk of any behavior change is yours. Nothing
updates `engine/` automatically; a project stays on the version it shipped with
until you choose to re-copy.
