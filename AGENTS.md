# AGENTS.md

## Cursor Cloud specific instructions

LogicSim is a single C++17 desktop GUI application (Dear ImGui + GLFW + OpenGL) — a
graph-based digital logic circuit simulator. There is no backend/server, no test
suite, and no linter configured; the only meaningful checks are "it builds" and
"it runs".

### Building / running

- Build is CMake-driven and pulls GLFW (3.4) and Dear ImGui (docking) via
  `FetchContent` at configure time (needs network on the first configure; cached
  under `build/_deps` afterwards).
- IMPORTANT: the default `c++`/`cc` is Clang 18, which fails to link (`cannot find
  -lstdc++`) in this image. Build with GCC instead — always pass
  `-DCMAKE_CXX_COMPILER=g++`. The update script already configures `build/` with g++.
- Configure: `cmake -S . -B build -DCMAKE_CXX_COMPILER=g++`
- Build: `cmake --build build -j"$(nproc)"` → produces `build/LogicSim`.
- Run the GUI on the existing VNC X server: `DISPLAY=:1 ./build/LogicSim`
  (OpenGL is software-rendered via Mesa llvmpipe, which is sufficient for the
  3.3 core profile the app requests). Run it under tmux if you want it to persist.

### Behavior notes

- Interact via the desktop (VNC): the left "Components" palette places parts
  (click a palette button to arm placement, then click on the canvas). Wire parts
  by dragging from an output pin to an input pin. Toggle a `Switch` by selecting it
  and clicking "Set VDD"/"Set GND" in the right-hand Properties panel; wire/pin
  colors update live (blue = LOW/GND, green = HIGH/VDD).
- Known quirk: the `LED` body does not visibly light even when its input pin shows
  the HIGH (green) color; the wire/pin propagation is the reliable visual indicator
  of simulation state.
