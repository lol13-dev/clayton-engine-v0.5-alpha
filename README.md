# Clayton Engine

GPU-Accelerated C++ Audio Engine & Visualizer

Clayton Engine is a custom, hardware-accelerated C++ audio engine and real-time FFT (Fast Fourier Transform) spectrum visualizer. Built with a focus on low-level memory management, thread safety, and responsive UI design, it transforms raw audio data into fluid, real-time graphical representations. Created by Clayton the Nerdy Lab (JUST A SMALL COMPANY).

## 📑 Table of Contents

* [Core Features](#-core-features)
* [What's New in v0.9.2](#-whats-new-in-v092)
* [Project Structure](#-project-structure)
* [Technical Architecture](#-technical-architecture)
* [Building & Running](#-building--running)
* [Roadmap](#-roadmap)

## ✨ Core Features

* 🎛️ **Real-Time FFT Visualizer**: Computes audio frequencies on the fly with smooth, asymmetric lerping (fast attack, slow decay) for organic, fluid bar movement.

* 🎨 **TrumFaster™ GPU Optimization**: Proprietary dynamic LOD framework that actively monitors frame pacing and auto-scales geometry complexity to guarantee a locked 60 FPS on lower-end hardware.

* 🌊 **Catmull-Rom Spline Rasterization**: Transforms rigid audio peaks into high-density, liquid-smooth neon wave geometry using advanced CPU interpolation.

* 📐 **Responsive Floating UI**: Built with Dear ImGui, featuring a mathematically centered, perfectly symmetrical HUD interface that scales dynamically.

* 🔄 **Playback State Machine**: Fully integrated `Normal`, `Repeat All`, `Repeat 1`, and `Shuffle` (RNG seeded) playback modes.

## 🚀 What's New in v0.9.0 (The "Cross-Platform" Update)

Version 0.9.0 completely overhauls the build architecture, migrating from hardcoded Apple scripts to a universal, industry-standard CMake pipeline.

### Architectural Leaps

* **CMake Build System**: The engine can now be natively compiled on Windows (Visual Studio/MSVC), Linux (GCC), and macOS (Clang) without altering a single line of terminal code.

* **OS-Aware Native Dialogs**: Replaced the legacy AppleScript folder browser with a dynamic C++ preprocessor system (`#ifdef _WIN32`, `#elif __linux__`). The engine now automatically triggers PowerShell on Windows and Zenity on Linux for a seamless native UX.

* **TrumFaster™ Integration**: Added UI telemetry overlays and dynamic manual overrides for the TrumFaster engine.

## 📂 Project Structure

```
ClaytonEngine/
├── CMakeLists.txt          # Universal Build Blueprint
├── assets/                 # Audio files and loaded MP3s
├── build/                  # Compiled binary executables
├── src/                    # Main Engine Source
│   ├── audio/              # Miniaudio logic & FFT processors
│   ├── core/               # Engine loop & State machines
│   ├── renderer/           # OpenGL, GLFW, and ImGui rendering
│   └── trumfaster/         # GPU Optimization Framework
├── third_party/            # External libraries (ImGui, GLM, GLFW)
└── tools/                  # Isolated developer scripts (DevDownloader)
```

## 🏗️ Technical Architecture

Clayton Engine separates its logic to maintain a flawless 60 FPS visual experience:

**The Audio Worker (Background Thread)**: Powered by the low-level Miniaudio API. Handles raw byte streaming, MP3 decoding, and custom software volume multiplication.

**The Graphics Worker (Main Thread)**: Powered by OpenGL, GLFW, and ImGui. Handles heavy FFT windowing calculations, renders the UI, and captures human input.

**The TrumFaster™ Optimizer**: Dynamic level-of-detail (LOD) system that intelligently scales GPU geometry complexity based on real-time frame pacing metrics, ensuring consistent 60 FPS performance across all hardware tiers.

## 💻 Building & Running

### Prerequisites

* CMake (v3.15+)
* C++17 Compiler (`clang++`, `g++`, or `MSVC`)
* Windows Users: Ensure `glfw3.lib` is placed in `third_party/glfw/lib/`

### 1. The Universal Build (All Platforms)

Instead of a massive terminal string, Clayton Engine now builds using standard CMake generation:

```bash
# 1. Create a build directory
mkdir build
cd build

# 2. Generate the native project files (Visual Studio, Makefiles, or Xcode)
cmake ..

# 3. Compile the engine
cmake --build .

# 4. Run the engine!
./ClaytonEngine
```

### 2. Compile & Run the Dev Downloader Tool

```bash
# The toolchain remains isolated from the main CMake project
clang++ -std=c++17 tools/DevDownloader.cpp -o build/ClaytonDevTool
./build/ClaytonDevTool
```

## 🗺️ Roadmap (Upcoming in v1.0 - The "Release Candidate")

* [ ] **GPU Compute Shaders**: Move the FFT math entirely to the graphics card to achieve 1000+ FPS simulation speeds.

* [ ] **Drag & Drop API**: Allow users to drag MP3 files directly from their OS into the ImGui window to load them instantly.

* [ ] **Standalone Binaries**: Package `.exe` and `.app` bundles for public distribution.