# Clayton Engine 🎵
**Version:** v0.7.1 (Alpha)  
**Platform:** macOS / Windows / Linux (Cross-Platform C++)
**Created by:** Clayton the Nerdy Lab.

Clayton Engine is a custom, hardware-accelerated C++ audio engine and real-time FFT (Fast Fourier Transform) spectrum visualizer. Built with a focus on low-level memory management, thread safety, and responsive UI design, it transforms raw audio data into fluid, real-time graphical representations.

## ✨ Core Features
* **Real-Time FFT Visualizer:** Computes audio frequencies on the fly with smooth, asymmetric lerping (fast attack, slow decay) for organic, fluid bar movement.
* **Responsive Floating UI:** Built with Dear ImGui, featuring a mathematically centered "pill" interface that scales dynamically with the window size.
* **Custom Software Volume Mixer:** Bypasses OS-level hardware restrictions (like macOS Bluetooth limitations) by multiplying raw audio streams in the data callback, unlocking up to **200% Overdrive**.
* **Continuous Playback:** Automatically detects the end of an audio stream and seamlessly transitions to the next track in the directory, gracefully shutting down at the end of the playlist.
* **Safe Asynchronous Seeking:** Interactive Seek Bar that pauses the audio thread strictly upon mouse-release to prevent memory corruption.

---

## 🚀 What's New in v0.7.1 (The "Stability Update")
Version 0.7.1 is a major architectural milestone focused on thread safety, UI polish, and squashing critical memory bugs.

### 🛠️ Bug Fixes & Patches
* **Defeated Thread Collision (Segmentation Fault):** Fixed a severe race condition where the UI Graphics Worker and Background Audio Worker simultaneously fought over the MP3 decoder memory during track seeking. 
* **Fixed "Ghost UI" Overlapping:** Resolved ImGui Z-fighting and overlapping elements by strictly enforcing `SameLine` spacing and properly managing UI component lifecycles.
* **Patched ImGui Stack Crashes:** Secured missing `PopItemWidth()` and `PopStyleVar()` commands to prevent intentional assertion crashes from ImGui's stack memory manager.
* **Resolved "Playing Silence" Loop:** Upgraded the playback detection logic to check current timestamps against track duration (`trackDuration - 0.1f`), fixing a bug where Miniaudio would infinitely pump silent frames at the end of a track.
* **Fixed Volume State Reset:** The engine now intelligently remembers the user's software volume multiplier and automatically reapplies it when switching tracks.

---

## 🏗️ Technical Architecture
Clayton Engine separates its logic into two distinct threads to maintain a flawless 60 FPS visual experience without interrupting audio playback:
1. **The Audio Worker (Background Thread):** Powered by the low-level **Miniaudio API**. Handles raw byte streaming, MP3 decoding, and custom software volume multiplication directly in the data callback.
2. **The Graphics Worker (Main Thread):** Powered by **OpenGL, GLFW, and ImGui**. Handles heavy FFT windowing calculations, renders the UI, and captures human input.

---

## 🗺️ Roadmap (Upcoming in v0.8)
* **Drag-and-Drop Asset Loading:** Allow users to drag MP3 files directly into the GLFW window to instantly add them to the active stream.
* **Visual Playlist Manager:** A dedicated, toggleable ImGui window to view the upcoming queue and select specific tracks.
* **Theme Engine:** Dynamic UI color changes based on the album art or dominant frequencies of the current song.

---

## 💻 Building & Running
*(Note: Requires a C++17 compiler, OpenGL, and GLFW installed).*

**To Compile (Debug Mode with AddressSanitizer):**
```bash
clang++ -std=c++17 src/**/*.cpp -g -fsanitize=address -o build/ClaytonEngine_v0.7.1 -framework OpenGL -framework Cocoa

**To RUN it:**
```bash
g++ -std=c++17 -DGL_SILENCE_DEPRECATION src/main.cpp src/core/Engine.cpp src/audio/AudioPlayer.cpp src/audio/FFT.cpp src/renderer/Window.cpp src/renderer/SpectrumRenderer.cpp third_party/imgui/imgui.cpp third_party/imgui/imgui_draw.cpp third_party/imgui/imgui_tables.cpp third_party/imgui/imgui_widgets.cpp third_party/imgui/backends/imgui_impl_glfw.cpp third_party/imgui/backends/imgui_impl_opengl3.cpp -o build/ClaytonEnginev0.7.1_Alpha -I./third_party/glfw/include -I./third_party -I./third_party/glm -I./third_party/imgui -I./third_party/imgui/backends -L./third_party/glfw/lib -lglfw3 -lpthread -framework Cocoa -framework OpenGL -framework IOKit -framework CoreVideo