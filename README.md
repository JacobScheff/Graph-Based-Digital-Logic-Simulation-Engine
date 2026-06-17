# LogicSim: Graph-Based Digital Logic Simulation Engine

LogicSim is an interactive, graph-based digital logic circuit simulator built in C++17. It features an intuitive graphical user interface (GUI) powered by ImGui, allowing users to design, visualize, and simulate complex digital logic circuits in real-time.

## Features

- **Interactive Canvas:** Place, connect, and organize logic gates and components via a drag-and-drop node-based interface.
- **Event-Driven Simulation Engine:** Accurately simulates circuit behavior, propagation delays, and logic states using a robust timing wheel architecture.
- **Extensive Component Library:** Includes standard logic gates, inputs/outputs (IO), power rails, and support for creating custom components.
- **Serialization:** Save and load circuit designs seamlessly using JSON.
- **Modern UI:** Built on the ImGui docking branch, providing a customizable, multi-window layout.

## Dependencies

The project relies on the following key technologies and libraries:

- **C++17:** Modern C++ standard required for building the engine.
- **CMake (3.14+):** Build system generator.
- **OpenGL:** For rendering the UI and canvas.
- **GLFW (3.4):** Window creation and input handling (fetched automatically via CMake).
- **Dear ImGui (Docking Branch):** Immediate mode GUI library for the interface (fetched automatically via CMake).
- **nlohmann/json:** Single-header library for JSON serialization (`json.hpp` included in the source).

## Building the Project

This project uses CMake to handle dependencies via `FetchContent` and generate build files.

### Prerequisites
- A modern C++ compiler supporting C++17 (e.g., GCC, Clang, MSVC).
- CMake 3.14 or newer.
- OpenGL development libraries (on Linux/WSL).

### Build Instructions

1. Clone the repository:
   ```bash
   git clone <repository_url>
   cd "Graph-Based Digital Logic Simulation Engine"
   ```

2. Create a build directory and configure the project:
   ```bash
   mkdir build
   cd build
   cmake ..
   ```

3. Build the executable:
   ```bash
   cmake --build .
   ```

## Usage

After building the project, run the `LogicSim` executable generated in the `build` directory:

```bash
./LogicSim
```

- **Canvas Interaction:** Use the mouse to navigate the canvas, place new components from the library, and drag connections between pins.
- **Simulation Control:** The simulation engine continuously evaluates the state of the circuit. Interact with IO components (like switches or buttons) to see real-time updates to logic states.

## Architecture Overview

- **App:** The main application entry point, managing the GLFW window, ImGui initialization, and the main loop.
- **Canvas (`Canvas.cpp`, `Canvas.hpp`):** Manages the visual node graph, rendering of components, wires, and user interactions.
- **Simulator (`Simulator.cpp`, `Simulator.hpp`):** The core engine responsible for evaluating logic networks and propagating states.
- **TimingWheel:** Handles scheduling of logic events to accurately simulate gate propagation delays.
- **Components (`Gates.hpp`, `IO.hpp`, `CustomComponent.cpp`):** Definitions for various logic primitives and user-defined sub-circuits.
- **Net & Pin:** Represent electrical connections and connection points on components.