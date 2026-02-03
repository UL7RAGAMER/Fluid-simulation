# Fluid Simulation

A real-time 2D fluid simulation using OpenGL Compute Shaders and Particle-based Lagrangian methods (SPH - Smoothed Particle Hydrodynamics).

## Features

- **Compute Shader Physics**: All physics calculations (Density, Pressure, Force integration) are performed on the GPU using Compute Shaders for high performance.
- **Interactive**:
  - Real-time parameter tuning via ImGui.
  - Mouse interaction to push fluid particles.
- **Rendering**: Instanced rendering of particles for efficient visualization.
- **Grid Optimization**: Spatial hashing using a grid to optimize neighbor searching in the simulation.

## Project Structure

The project is organized as follows:

- **FluidSimulation/src/**: Contains the C++ source code.
  - `main.cpp`: Entry point, window setup, and main loop.
  - `Simulation.cpp/h`: Manages the physics simulation, SSBOs, and compute shaders.
  - `Renderer.cpp/h`: Handles rendering of particles and visual elements.
  - `Shader.h`: Utility class for loading and compiling shaders.
- **FluidSimulation/assets/shaders/**: GLSL shader files (`.comp` for compute, `.shader` for rendering).
- **FluidSimulation/Dependencies/**: Third-party libraries (GLEW, GLFW, GLM, ImGui, stb_image).

## Dependencies

The project includes the following dependencies in the `Dependencies` folder:
- **GLFW**: Window and input management.
- **GLEW**: OpenGL extension loading.
- **GLM**: Mathematics library.
- **ImGui**: Immediate Mode GUI.
- **stb_image**: Image loading (included but currently unused).

## Build Instructions (Windows - Visual Studio)

1.  Open `FluidSimulation.sln` in Visual Studio 2022 (or compatible version).
2.  Select the build configuration (Debug or Release) and platform (x64 recommended).
3.  Build the solution (Ctrl+Shift+B).
4.  Run the application (F5).

**Note**: Ensure your graphics driver supports **OpenGL 4.3** or higher, as Compute Shaders are required.

## Controls

- **GUI**: A control panel allows you to adjust simulation parameters in real-time:
  - **Gravity**: Strength of gravity.
  - **Rest Density**: Target density for the fluid.
  - **Gas Constant**: Pressure stiffness.
  - **Viscosity**: Fluid thickness/resistance to flow.
  - **Particle Count**: Adjust the number of particles (up to 50,000).
- **Mouse**: Click and drag in the simulation window to apply a repulsive force to particles.

## Technical Details

The simulation uses a density-based pressure solver (SPH).
1.  **Grid Clear & Count**: Particles are mapped to grid cells to optimize neighbor lookup.
2.  **Density Pass**: Calculates density and pressure for each particle based on neighbors.
3.  **Force Pass**: Applies pressure, viscosity, gravity, and boundary forces, then integrates position.
4.  **Render**: Draws particles using instanced triangle fans.
