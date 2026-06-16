// UPGRADED VERSION.
// BETTER REAL-TIME PERFORMANCE WITH RING BUFFER AND FFT SUPPORT. SEE src/core/Engine_v1.cpp for the original version without these features.
// Engine.cpp
#include "Engine.h"
#include "../audio/AudioPlayer.h"
#include "../audio/FFT.h"
#include "../renderer/Window.h"
#include "../renderer/SpectrumRenderer.h"
#include "../../third_party/imgui/imgui.h"
#include "../../third_party/imgui/backends/imgui_impl_glfw.h"
#include "../../third_party/imgui/backends/imgui_impl_opengl3.h"

// SEEN IN: src/audio/AudioPlayer.h
#include <iostream>
#include <thread>
#include <chrono>
#include <vector>
#include <cmath>
#include <filesystem>

namespace fs = std::filesystem; // <- Create a namespace for filesystem operations.
// =====================================
// CONSTRUCTOR
// =====================================
Engine::Engine()
{
    std::cout << "[CONSTRUCTOR] Engine Object Created.\n";
}

// =====================================
// DESTRUCTOR
// =====================================
Engine::~Engine()
{
    std::cout << "[DESTRUCTOR] Engine Object Destroyed.\n";
}

// =====================================
// INITIALIZE ENGINE
// =====================================
void Engine::Initialize()
{
    std::cout << "Clayton Engine Initialized.\n";
}

// =====================================
// MAIN ENGINE LOOP
// =====================================
void Engine::Run()
{
    //-----------------------------------
    // DYNAMIC TRACK SELECTION MENU.
    // -----------------------------------

    std::vector<std::string> playlist;
    std::string assetsPath = "assets";

    // SCAN the assets folder for all any .mp3 files
    if (fs::exists(assetsPath) && fs::is_directory(assetsPath))
    {
        for (const auto &entry : fs::directory_iterator(assetsPath))
        {
            if (entry.path().extension() == ".mp3")
            {
                playlist.push_back(entry.path().string());
            }
        }
    }

    // FALLBACK if the assets folder is EMPTY or MISSING.
    if (playlist.empty())
    {
        std::cout << "[ENGINE ERROR] No .mp3 files found in 'assets/' folder!\n";
        return;
    }

    // PRINT a clean interative console selector.
    std::cout << "==================================================\n";
    std::cout << "=   Clayton Engine v0.7 Alpha PLAYLIST SELECTOR  =\n";
    std::cout << "==================================================\n";
    for (size_t i = 0; i < playlist.size(); i++)
    {
        // EXTRACT just filename for a clear display look.
        std::string filename = fs::path(playlist[i]).filename().string();
        std::cout << "[" << i + 1 << "] " << filename << "\n";
    }

    int choice = 0;
    std::cin >> choice;

    // VALIDATE input selection boundaries
    if (choice < 1 || choice > static_cast<int>(playlist.size()))
    {
        std::cout << "[ENGINE ERROR] Invalid selection. DEFAULTING to track 1.\n";
        choice = 1;
    }

    int currentTrackIndex = choice - 1; // Remember where I in the playlist!
    std::string selectedTrackPath = playlist[currentTrackIndex];
    std::string cleanTrackName = fs::path(selectedTrackPath).filename().stem().string();

    //------------------------------------
    // Create modules
    //------------------------------------
    AudioPlayer player;
    FFT fft;

    // -----------------------------------
    // 1. CREATE a Window.
    // -----------------------------------
    Window window(1280, 720, "WaveformVisual Online v0.7.1 (Alpha) - Powered by Clayton Engine.");
    if (!window.Initialize())
    {
        std::cout << "[ENGINE] Failed to initialize window. Exiting...\n";
        return;
    }

    // -----------------------------------
    // 2. CREATE a Spectrum Renderer.
    // -----------------------------------
    SpectrumRenderer spectrumRenderer;
    spectrumRenderer.Initialize();

    // ==========================================
    // IMGUI PHASE 1: INITIALIZATION
    // ==========================================
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    ImGui::StyleColorsDark();
    
    // Connect ImGui to your Mac Window and OpenGL
    ImGui_ImplGlfw_InitForOpenGL(window.GetGLFWWindowPointer(), true);
    ImGui_ImplOpenGL3_Init("#version 330 core");

    //------------------------------------
    // Load MP3 Asset Stream (NOW IT'S DYNAMIC)
    //------------------------------------
    if (player.Load(selectedTrackPath))
    {
        player.Play();
        std::cout << "[ENGINE] Now streaming: " << cleanTrackName << "\n";
    }
    else
    {
        std::cout << "[ENGINE] Failed to load audio. Exiting...\n";
        return;
    }

    const int TARGET_FPS = 60;
    const int FRAME_TIME_MS = 1000 / TARGET_FPS;
    const size_t FFT_WINDOW_SIZE = 1024;

    // NEW: Remembers if the user intentionally paused the music
    bool isUserPaused = false;

    // 1.0f is 100% volume. 2.0f allows the user to overdrive the audio to 200%!
    float currentVolume = 1.0f;

    // ASK the MP3 for its length ONCE before the starts.
    float trackDuration = player.GetDuration();

    //------------------------------------
    // UPDATE 1: Window-Controlled Loop
    //------------------------------------
    // I removed "&& player.IsPlaying()" so the app stays open when paused!
    while (window.IsOpen())
    {
        // ==========================================
        // NEW: AUTO-CLOSE LOOPS
        // ==========================================
        // If the music stopped, AND the user didn't click pause... the track is over!
        if (!isUserPaused && !player.IsPlaying()){
            std::cout << "[ENGINE] Track finished playing. Auto-closing Alpha build....\n";
            break; // This immediately exits the while loop and runs your Shutdown() code!
        }

        window.Clear(0.0f, 0.0f, 0.0f, 1.0f);

        // ==========================================
        // IMGUI PHASE 2: START NEW UI FRAME
        // ==========================================
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        // QUERY the actual dynamic size of the WINDOW!
        ImVec2 viewportSize = ImGui::GetMainViewport()->Size; 

        // ==========================================
        // NEW: ImGui Green Gradient BACKGROUND
        // ==========================================
        // I use ImGui's background draw list to paint a gradient over the black OpenGL canvas.
        ImU32 colorTop = IM_COL32(26, 60, 40, 255);     // Premium Dark Green (#1A3C28)
        ImU32 colorBottom = IM_COL32(10, 25, 17, 255);  // Deep Midnight Green (#0A1911)

        ImGui::GetBackgroundDrawList()->AddRectFilledMultiColor(
            ImVec2(0, 0), viewportSize,                     // START at top-left, stretch to bottom-right
            colorTop, colorTop, colorBottom, colorBottom    // Apply color (TopLeft, TopRight, BottomRight, BottomLeft)
        );

        // ==========================================
        // PLAY/PAUSE FREEZE LOGIC
        // ==========================================
        // 'static' means this variable remembers its data even when the frame restarts.
        // It holds the "frozen" shape of the bars when you hit pause!
        static std::vector<float> frozenFrequencies(1024, 0.0f);

        // -----------------------------------
        // ONLY crunch the heavy FFT math if the music is actually playing
        // -----------------------------------
        if (player.IsPlaying()){
            std::vector<float> samples = player.GetLatestSamples(FFT_WINDOW_SIZE);

            for (size_t i = 0; i < samples.size(); i++)
            {
                float windowValue = 0.5f * (1.0f - std::cos((2.0f * M_PI * i) / (FFT_WINDOW_SIZE - 1)));
                samples[i] *= windowValue;
            }

            frozenFrequencies = fft.Process(samples);
            frozenFrequencies[0] = 0.0f;
            frozenFrequencies[1] = 0.0f;
        }

        const size_t DISPLAY_BARS = 16;
        size_t usableBins = 256;
        size_t binsPerBar = usableBins / DISPLAY_BARS;

        // ==========================================
        // New: TRUE RESPONSIVE MATH (PERCENTAGES)
        // ==========================================
        // Dynamic Width: THE VISUALIZER takes up 80% of the window width
        float maxAvailableWidth = viewportSize.x * 0.8f;

        // PUT a cap on it so it doesn't scretch too far on ultra-wide monitors
        if (maxAvailableWidth > 1200.0f) maxAvailableWidth = 1200.0f;

        // Divide the available space evenly
        float barSpacing = maxAvailableWidth / DISPLAY_BARS;

        // THE BAR takes up 55% of its given slot, leaving a 45% empty cap
        float barWidth = barSpacing * 0.55f;

        float totalBarsWidth = (DISPLAY_BARS - 1) * barSpacing + barWidth;
        float startPosX = (viewportSize.x - totalBarsWidth) * 0.5f;

        // ==========================================
        // THE FIX: SMOOTHING ARRAY
        // ==========================================
        // 'static' means this array survives between frames so it remembers the heights!
        static std::vector<float> smoothHeights(DISPLAY_BARS, 0.0f);

        for (size_t b = 0; b < DISPLAY_BARS; b++)
        {
            float binAverage = 0.0f;
            for (size_t j = 0; j < binsPerBar; j++)
            {
                size_t index = 2 + (b * binsPerBar) + j;
                if (index < frozenFrequencies.size())
                {
                    binAverage += frozenFrequencies[index];
                }
            }
            binAverage /= binsPerBar;

            float eqBoost = 1.0f + (b * 0.15f);
            float boostedAverage = binAverage * eqBoost;

            const float SENSITIVITY = 15.0f;
            float logValue = std::log10(boostedAverage * SENSITIVITY + 1.0f);
            const float MAX_LOG_VALUE = 2.4f;

            // This is the "Target" height from the RAW AUDIO.
            float targetHeight = logValue / MAX_LOG_VALUE;
            if (targetHeight > 1.0f) targetHeight = 1.0f;
            if (targetHeight < 0.0f) targetHeight = 0.0f;

            // ==========================================
            // THE UNIVERSAL FIX: ASYMMETRIC LEEPING  (ATTACK OR DELAY)
            // ==========================================
            // This setup works perfectly for EDM, Classical, Jazz, and Rock automatically.
            float attackFactor = 0.40f;
            float decayFactor = 0.06f;

            if (targetHeight > smoothHeights[b]) {
                smoothHeights[b] += (targetHeight - smoothHeights[b]) * attackFactor;
            } else {
                smoothHeights[b] += (targetHeight - smoothHeights[b]) * decayFactor;
            }

            // ==========================================
            // NEW: DYNAMIC BAR HEIGHTS & ANCHORING
            // ==========================================
            // 2. DYNAMIC HEIGHT: Bars can grow up to 45% of the window's total height
            float maxBarHeight = viewportSize.y * 0.45f;
            float actualHeight = smoothHeights[b] * maxBarHeight;

            // 3. MINIMUM HEIGHT: Even the quiet data "resting dots" scale down on tiny screens
            float minHeight = viewportSize.y * 0.02f;
            if (actualHeight < minHeight) actualHeight = minHeight;
            float xPixelPos = startPosX + (b * barSpacing);

            // 4. DYNAMIC Y-ANCHOR: Pin the bottom of the bars precisely 75% down the screen
            float bottomY = viewportSize.y * 0.75f; 
            float topY = bottomY - actualHeight;

            // Paint the bars OVER the gradient, with built-in rounded corners!
            float hue = 0.5f - (smoothHeights[b] * 0.5f);
            if (hue < 0.0f) hue = 0.0f; // CLAMP it just in case.
            ImU32 dynamicColor = ImColor::HSV(hue, 0.9f, 1.0f);

            ImGui::GetBackgroundDrawList()->AddRectFilled(
                ImVec2(xPixelPos, topY),
                ImVec2(xPixelPos + barWidth, bottomY),
                dynamicColor, // Vibrant Cyan (#00E5FF)
                15.0f // Perfectly rounded caps!
            );
        }

        // ==========================================
        // IMGUI PHASE 3: "NOW PLAYING" TEXT
        // ==========================================
        std::string nowPlayingText = "Now Playing: " + cleanTrackName;

        // CALCULATE exactly how wide the text is so I can center it PERFECTLY
        ImVec2 textSize = ImGui::CalcTextSize(nowPlayingText.c_str());
        float textPosX = (viewportSize.x - textSize.x) * 0.5f;

        ImGui::SetNextWindowPos(ImVec2(textPosX, 30.0f)); // <- 30 PIXELS from the top edge.

        // CREATE an invisible background window just to hold the text.
        ImGui::Begin("Metadata", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoInputs);

        // DRAW the text in a light gray color
        ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "%s", nowPlayingText.c_str());

        ImGui::End();

        // ==========================================
        // IMGUI PHASE 4: RESPONSIVE "PILL" INTERFACE
        // ==========================================
        float pillWidth = 500.0f;
        float pillHeight = 150.0f;

        // RESPONSIVE MATH: Center X, and lock Y to 60 pixels above the BOTTOM edge.
        float pillPosX = (viewportSize.x - pillWidth) * 0.5f;
        float pillPosY = (viewportSize.y - pillHeight) - 40.0f;

        ImGui::SetNextWindowPos(ImVec2(pillPosX, pillPosY)); 
        ImGui::SetNextWindowSize(ImVec2(pillWidth, pillHeight));

        // Create a dark, rounded window container
        ImGui::Begin("Controls", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoBackground);
        
        // Center the buttons inside the pill
        // float centerOffSet = (pillWidth - 354.0f) * 0.5f; <- STILL NOT WORKING
        ImGui::SetCursorPos(ImVec2(70.0f, 15.0f)); 
        // [C++ LEARNING] 'PushStyleVar' changes the internal ImGui drawing rules.
        // 'FrameRounding, 10.0f' curves the corners of every button drawn after this line!
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 10.0f);

        // ==================== PREV BUTTON ====================
        if (ImGui::Button("Prev", ImVec2(80, 50))) {
            player.Stop();
            // Loops BACKWARDS cleanly EVEN if you are on Track 1.
            currentTrackIndex = (currentTrackIndex - 1 + playlist.size()) % playlist.size();
            selectedTrackPath = playlist[currentTrackIndex];
            cleanTrackName = fs::path(selectedTrackPath).filename().stem().string();
            player.Load(selectedTrackPath);
            player.SetVolume(currentVolume);
            trackDuration = player.GetDuration();
            player.Play();
            isUserPaused = true;
        }

        // [C++ LEARNING] Forces exactly 10 pixels of space between buttons
        ImGui::SameLine(0.0f, 10.0f);

        // ==================== STOP BUTTON ====================
        if (ImGui::Button("STOP", ImVec2(80, 50))) {
            player.Stop();
            // The bars will now gracefully drop to the bottom and wait!
            // [C++ LEARNING] Loop through the array and set every frequency back to 0.0!
            // Because we set it to 0, the Lerp math will gracefully animate the bars falling down.

            // [C++ LEARNING] Reloading the path forces the audio buffer back to 0:00!
            // Now when you hit PLAY next, it will start from the very beginning.
            player.Load(selectedTrackPath); 
            // [C++ LEARNING] Re-apply the volume state immediately so the new track doesn't blast at 100%!
            player.SetVolume(currentVolume);
            isUserPaused = true;

            for (size_t i = 0; i < frozenFrequencies.size(); i++) {
                frozenFrequencies[i] = 0.0f;
            }
        }

        ImGui::SameLine(0.0f, 10.0f); // C++ LEARNING WHILE CODE: Forces the next button to be on the SAME horizontal row!
        
        // ==================== Dynamic Play/Stop Button ====================
        if (player.IsPlaying()) {
            if (ImGui::Button("PAUSE", ImVec2(90, 50))) {
                player.Stop();
                // [C++ LEARNING] Re-apply the volume state immediately so the new track doesn't blast at 100%!
                player.SetVolume(currentVolume);
                isUserPaused = true; // Tell the engine this was intentional!
            }
        } else {
            if (ImGui::Button("PLAY", ImVec2(90, 50))) {
                player.Play();
                // [C++ LEARNING] Re-apply the volume state immediately so the new track doesn't blast at 100%!
                player.SetVolume(currentVolume);
                isUserPaused = false; // Music is running naturally again
            }
        }
        
        ImGui::SameLine(0.0f, 10.0f);

        // ==================== NEXT BUTTON ====================
        if (ImGui::Button("Next", ImVec2(80, 50))) {
            player.Stop();
            // Loops back to track 1 if you hit Next on the final track
            currentTrackIndex = (currentTrackIndex + 1) % playlist.size();
            selectedTrackPath = playlist[currentTrackIndex];
            cleanTrackName = fs::path(selectedTrackPath).filename().stem().string();
            player.Load(selectedTrackPath);
            // [C++ LEARNING] Re-apply the volume state immediately so the new track doesn't blast at 100%!
            player.SetVolume(currentVolume);
            trackDuration = player.GetDuration();
            player.Play();
            isUserPaused = false;
        }

        // ==================== SEEK BAR (NEW) ====================
        // float trackDuration = player.GetDuration(); <- NOT USED DUE TO WAS CRASHING 
        // float currentPos = player.GetCurrentPosition(); <- NOT USED DUE TO CRASHING AGAIN
        static float uiSliderPos = 0.0f;
        static bool isDraggingSeek = false;
        
        // ONLY update the UI from the real audio if I not DRAGGING the MOUSE
        if (!isDraggingSeek){
            uiSliderPos = player.GetCurrentPosition();
        }
        
        // MATH to CONVERT RAW seconds into minutes and SECONDS.
        int curM = (int)uiSliderPos / 60;
        int curS = (int)uiSliderPos % 60;
        int totM = (int)trackDuration / 60;
        int totS = (int)trackDuration % 60;

        // FORMAT the text to look like "1:23 / 3:45"
        char timeLabel[64];
        snprintf(timeLabel, sizeof(timeLabel), "%d:%02d / %d:%02d", curM, curS, totM, totS);

        ImGui::SetCursorPos(ImVec2(70.0f, 75.0f)); // PUT SEEK BAR at Y: 75
        ImGui::PushItemWidth(360.0f);

        // DRAW the SLIDER, but I REMOVED the IMMEDIATE SEEK command.
        ImGui::SliderFloat("##SeekBar", &uiSliderPos, 0.0f, trackDuration, timeLabel);

        if (ImGui::IsItemActive()) {
            isDraggingSeek = true;
        }

        // SAFELY seek ONLY when the user LET'S GO of THE MOUSE.
        if (ImGui::IsItemDeactivatedAfterEdit()) {
            player.Stop(); // HALT the AUDIO THREAD.
            player.SeekToPosition(uiSliderPos); // MOVE the MP3 playhead.
            if (!isUserPaused) player.Play(); // RESUME playing seamlessly.
            isDraggingSeek = false;
        } else if (ImGui::IsItemDeactivated()) {
            isDraggingSeek = false; // MOUSE released without changing anything.
        }
        ImGui::PopItemWidth();

        // ============= VOLUME OVERDRIVE SLIDER ================
        // Move the "cursor" down to Y: 75 so it sits nicely under the buttons.
        // We keep X: 70 so its left edge aligns perfectly with the 'Prev' button.
        ImGui::SetCursorPos(ImVec2(70.0f, 110.0f));

        // PushItemWidth locks the slider's length to exactly 360 pixels 
        // so it perfectly matches the width of the 4 buttons above it!
        ImGui::PushItemWidth(360.0f);

        // SliderFloat min is 0.0f (mute), max is 2.0f (200% overdrive).
        if (ImGui::SliderFloat("##Volume", &currentVolume, 0.0f, 2.0f, "Volume: %.2fx")){
            // When the USER drags the slider, this BLOCKS TRIGGERS!
            player.SetVolume(currentVolume);
        }
        ImGui::PopItemWidth();

        ImGui::PopStyleVar();
        ImGui::End();

        // ==========================================
        // IMGUI PHASE 4: RENDER TO SCREEN
        // ==========================================
        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        // Swap the video buffers and push the pixel data to your monitor
        window.Update();
        std::this_thread::sleep_for(std::chrono::milliseconds(FRAME_TIME_MS));
    }

    // ==========================================
    // IMGUI PHASE 5: CLEAN SHUTDOWN
    // ==========================================
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    player.Stop();
}

// =====================================
// SHUTDOWN ENGINE
// =====================================
void Engine::Shutdown()
{
    std::cout << "Engine Shutdown.\n";
}