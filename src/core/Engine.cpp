// UPGRADED VERSION (ALMOST DONE).
// BETTER REAL-TIME PERFORMANCE WITH RING BUFFER AND FFT SUPPORT. SEE src/core/Engine_v1.cpp for the original version without these features.
// Engine.cpp
#include "Engine.h"
#include "../audio/AudioPlayer.h"
#include "../audio/FFT.h"
#include "../renderer/Window.h"
#include "../renderer/SpectrumRenderer.h"
#include "../trumfaster/TrumFaster.h"
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
#include <atomic> // <- For thread-safe operations.
#include <algorithm> // <- For standard algorithms like std::sort.
// FOR REPEAT, REPEAT ALL, SHUFFLE the music.
#include <cstdlib> // FOR rand(); math
#include <ctime>

#include <GLFW/glfw3.h> // REQUIRED for dynamic Framebuffer RESIZING.

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
    // -----------------------------------
    // DYNAMIC TRACK SELECTION MENU (CLEAN BOOT STATE).
    // -----------------------------------

    // UX FIX Step 1: START completely EMPTY. No hardcoded "assets" folder.
    std::vector<std::string> playlist;
    int currentTrackIndex = 0;
    std::string selectedTrackPath = "";
    std::string cleanTrackName = "";

    // -----------------------------------
    // 1. Create modules.
    // -----------------------------------
    AudioPlayer player;
    FFT fft;

    // -----------------------------------
    // 2. CREATE a Window.
    // -----------------------------------
    Window window(1280, 720, "WaveformVisual Online v0.9.2 (Alpha) - Powered by Clayton Engine.");
    if (!window.Initialize())
    {
        std::cout << "[ENGINE] Failed to initialize window. Exiting...\n";
        return;
    }

    // -----------------------------------
    // UX FIX: MINIMUM WINDOW SIZE (Anti-Squish).
    // -----------------------------------
    // LOCK the minimum dimensions to 900x600.
    // This Guarantees the 830px UI PIll NEVER GETS squished.
    // And the visualizer always has ENOUGH room to OVERRIDE.
    glfwSetWindowSizeLimits(window.GetGLFWWindowPointer(), 900, 600, GLFW_DONT_CARE, GLFW_DONT_CARE);

    // -----------------------------------
    // 3. CREATE a Spectrum Renderer.
    // -----------------------------------
    SpectrumRenderer spectrumRenderer;
    spectrumRenderer.Initialize();

    // ==========================================
    // IMGUI Phase 1: INITIALIZATION.
    // ==========================================
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    ImGui::StyleColorsDark();
    
    // Connect ImGui to your Mac Window and OpenGL
    ImGui_ImplGlfw_InitForOpenGL(window.GetGLFWWindowPointer(), true);
    ImGui_ImplOpenGL3_Init("#version 330 core");

    const int TARGET_FPS = 60;
    const int FRAME_TIME_MS = 1000 / TARGET_FPS;
    const size_t FFT_WINDOW_SIZE = 1024;

    // NEW: Remembers if the user intentionally paused the music
    bool isUserPaused = false;

    // DEFAULTING to 1.0f is 100% volume. 2.0f allows the user to overdrive the audio to 200%!
    float currentVolume = 1.0f;
    // ASK the MP3 for its length ONCE before the starts.
    float trackDuration = player.GetDuration();

    // ==========================================
    // NEW: PLAYBACK STATE MACHINE (THEN UPDATE 1: Window-Controlled Loop)
    // ==========================================
    // 0 = Normal, 1 = Repeat All, 2 = Repeat 1, 3 = Shuffle
    int playbackMode = 0;   // DEFAULTING to 1 (REPEAT ALL) since that's what I built.
    srand(time(NULL));      // "Seed" the randomizer using my Mac's internal lock.

    // DECLARE the VISUALIZER bars.
    std::vector<float> frozenFrequencies(1024, 0.0f);

    // ==========================================
    // NEW: TrumFaster, Adaptive Rendering & Frame Pacer.
    // ==========================================
    TrumFaster trumFaster(60); 

    // NEW: USER TOGGLE for TrumFaster.
    bool isTrumFasterEnabled = true;
    
    while (window.IsOpen())
    {
        trumFaster.StartFrame(); // START the STOPWATCH.
        // ==========================================
        // NEW FEATURE: CONTINUOUS PLAYBACK (AUTO-NEXT)
        // ==========================================
        // If the music stopped naturally (the user didn't click pause)... the track is over!
        if (!isUserPaused && trackDuration > 0.0f && !playlist.empty() && player.GetCurrentPosition() >= (trackDuration - 0.1f)){
            
            player.Stop(); // ENSURE the hardware is FULLY STOPPED.
            bool shouldPlayNext = true;
            std::cout << "[ENGINE] TRACK FINISHED. Auto-playing next track...\n";

            // CHECK the STATE MACHINE.
            if (playbackMode == 2) {
                // REPEAT: The index stays exactly the same. DO NOTHING WITH IT.
            } else if (playbackMode == 3) {
                // SHUFFLE: PICK a COMPLETELY random track index.
                currentTrackIndex = rand() % playlist.size();
            } else if (playbackMode == 0 && currentTrackIndex == playlist.size() - 1) {
                // NORMAL MODE: If we are on the last track, stop the music completely.
                isUserPaused = true;
                shouldPlayNext = false;
            } else {
                // REPEAT ALL (or Normal mode not at the end): Move forward 1 track
                currentTrackIndex = (currentTrackIndex + 1) % playlist.size();
            }

            // ONLY load the next track if Normal Mode didn't halt the player.
            if (shouldPlayNext) {
                // 1. LOAD and PLAY the next track.
                selectedTrackPath = playlist[currentTrackIndex];
                cleanTrackName = fs::path(selectedTrackPath).filename().stem().string();

                // 2. LOAD the NEW TRACK AND APPLY USER SETTINGS.
                player.Load(selectedTrackPath);
                player.SetVolume(currentVolume);
                trackDuration = player.GetDuration();

                // 3. START the MUSIC.
                player.Play();

            }

            // RESET the VISUALIZER bars.
            for (size_t i = 0; i < frozenFrequencies.size(); i++)
            {
                frozenFrequencies[i] = 0.0f;
            }
        }

        // ==========================================
        // NEW: RESPONSIVE FIX for PREVENT UI and Canvas Stretching.
        // ==========================================
        int fbWidth, fbHeight;
        glfwGetFramebufferSize(window.GetGLFWWindowPointer(), &fbWidth, &fbHeight);
        glViewport(0, 0, fbWidth, fbHeight);

        window.Clear(0.0f, 0.0f, 0.0f, 1.0f);

        // ==========================================
        // IMGUI Phase 2: START NEW UI FRAME.
        // ==========================================
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        // NOTE:QUERY the actual dynamic size of the WINDOW!
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

        // ==========================================
        // FPS & TrumFaster OVERLAY (Perfectly centered).
        // ==========================================
        std::string fpsText = "FPS: " + std::to_string((int)trumFaster.GetActualFPS());
        std::string tfStatusText = isTrumFasterEnabled ? " | TrumFaster: ON" : " | TrumFaster: OFF";
        std::string telemetryText = fpsText + tfStatusText;

        ImVec2 telemetrySize = ImGui::CalcTextSize(telemetryText.c_str());
        float telemetryPosX = (viewportSize.x - telemetrySize.x) * 0.5f;

        // POSITION it to 10 pixels from the top edge of THE SCREEN.
        ImGui::SetNextWindowPos(ImVec2(0.0f, 10.0f));
        ImGui::SetNextWindowSize(ImVec2(viewportSize.x, 30.0f));
        ImGui::Begin("Telemetry_Overlay", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoScrollbar);

        // FIXED: I must move the cursor to the CALCULATED center.
        ImGui::SetCursorPosX(telemetryPosX);

        // LET the user know if optimization is ACTIVE.
        if (isTrumFasterEnabled) {
            // GLOWS Green when optimized.
            ImGui::TextColored(ImVec4(0.0f, 0.8f, 0.2f, 1.0f), "%s", telemetryText.c_str());
        } else {
            // TURNS Red when running unoptimized.
            ImGui::TextColored(ImVec4(0.8f, 0.2f, 0.2f, 1.0f), "%s", telemetryText.c_str());
        }

        ImGui::End();

        // ==========================================
        // IMGUI Phase 3: RESPONSIVE "PILL" INTERFACE
        // ==========================================
        // I Wide the pill slightly to 760.0f so the toggle button FITS PERFECLY.
        float pillWidth = 830.0f;
        float pillHeight = 190.0f;
        // RESPONSIVE MATH: Center X, and lock Y to 60 pixels above the BOTTOM edge.
        float pillPosX = (viewportSize.x - pillWidth) * 0.5f;
        float pillPosY = (viewportSize.y - pillHeight) - 40.0f;

        // ==========================================
        // New: TRUE RESPONSIVE MATH (PERCENTAGES) AND VISUALIZER STATE MACHINE
        // ==========================================
        static int visualMode = 0; // 0 = CLASSIC BOTTOM, 1 = CENTER WAVEFORM, 2 = Neon Polyline.

        // TrumFaster: LOD Override (FIXED).
        int targetBars = (visualMode == 0) ? 16 : 64;
        TrumFasterProfile tfProfile;

        if (isTrumFasterEnabled) {
            tfProfile = trumFaster.GetOptimizedProfile(targetBars, visualMode);
        } else {
            // MANUAL OVERRIDE: IF TrumFaster is OFF, FORCE maximum QUALITY.
            tfProfile.activeBars = targetBars;
            tfProfile.enableShadows = true;
            tfProfile.lerpAttackSpeed = 0.92f;
        }

        const size_t DISPLAY_BARS = tfProfile.activeBars;
        
        // Dynamic Width: THE VISUALIZER takes up 80% of the window width
        float maxAvailableWidth = viewportSize.x * 0.8f;
        // PUT a cap on it so it doesn't scretch too far on ultra-wide monitors
        if (maxAvailableWidth > 1200.0f) maxAvailableWidth = 1200.0f;
        // Divide the available space evenly
        float barSpacing = maxAvailableWidth / DISPLAY_BARS;

        // THE BAR takes up 55% of its given slot, leaving a 45% empty cap
        float barWidth = (visualMode == 0) ? barSpacing * 0.55f : barSpacing * 0.45f;

        float totalBarsWidth = (DISPLAY_BARS - 1) * barSpacing + barWidth;
        float startPosX = (viewportSize.x - totalBarsWidth) * 0.5f;

        // ==========================================
        // THE FIX: SMOOTHING ARRAY
        // ==========================================
        // 'static' means this array survives between frames so it remembers the heights!
        static std::vector<float> smoothHeights(128, 0.0f);

        // Mode 2 (Polyline) REQUIRES keeping track on points.
        std::vector<ImVec2> mainLinePoints;
        std::vector<ImVec2> shadowLinePoints;
        std::vector<ImVec2> rawLinePoints; // NEW: Holds raw peaks for GPU Spline application.

        // v0.9.3 RESPONSIVE FIX: Anchor relative to the screen height, not the UI Pill!
        // This stops the polyline from flying off the top of the window when shrunk.
        float centerY = viewportSize.y * 0.55f;
        // float centerY = pillPosY - 180.0f; (DISABLED, BUT ENABLED IF THAT NEW CODE IS ERROR)

        if (visualMode == 2){
            mainLinePoints.push_back(ImVec2(startPosX -  40.0f, centerY));
            shadowLinePoints.push_back(ImVec2(startPosX - 40.0f, centerY + 6.0f));
        }

        float minLog = std::log10(2.0f);
        float maxLog = std::log10(256.0f);

        // ONE UNIFIED LOOP FOR ALL MATH.
        for (size_t b = 0; b < DISPLAY_BARS; b++)
        {
            float startLog = minLog + (maxLog - minLog) * ((float)b / DISPLAY_BARS);
            float endLog = minLog + (maxLog - minLog) * ((float)(b + 1) / DISPLAY_BARS);

            size_t startIndex = (size_t)std::pow(10.0f, startLog);
            size_t endIndex = (size_t)std::pow(10.0f, endLog);
            if (endIndex <= startIndex) endIndex = startIndex + 1;

            float binAverage = 0.0f;
            int count = 0;
            for (size_t j = startIndex; j < endIndex; j++)
            {
                // size_t index = 2 + (b * binsPerBar) + j;
                if (j < frozenFrequencies.size()) {
                    binAverage += frozenFrequencies[j];
                    count++;
                }
            }
            if (count > 0) binAverage /= count;

            float eqBoost = 1.0f + (b * 0.35f);
            float boostedAverage = binAverage * eqBoost;

            const float SENSITIVITY = 10.0f;
            float logValue = std::log10(boostedAverage * SENSITIVITY + 1.0f);
            const float MAX_LOG_VALUE = 2.8f;

            // This is the "Target" height from the RAW AUDIO.
            float targetHeight = logValue / MAX_LOG_VALUE;
            if (targetHeight > 1.0f) targetHeight = 1.0f;
            if (targetHeight < 0.0f) targetHeight = 0.0f;

            // ==========================================
            // THE UNIVERSAL FIX: ASYMMETRIC LEEPING (ATTACK OR DELAY)
            // ==========================================
            // EDM-optimized Asymmetric Lerping (Tied to TrumFaster profile).
            float attackFactor = tfProfile.lerpAttackSpeed; // it was 0.40f;
            float decayFactor = 0.07f; // it was 0.06f;

            if (targetHeight > smoothHeights[b]) {
                smoothHeights[b] += (targetHeight - smoothHeights[b]) * attackFactor;
            } else {
                smoothHeights[b] += (targetHeight - smoothHeights[b]) * decayFactor;
            }

            // ==========================================
            // NEW: DYNAMIC BAR HEIGHTS & ANCHORING
            // ==========================================
            // DYNAMIC HEIGHT: Bars can grow up to 45% of the window's total height
            float maxBarHeight = viewportSize.y * 0.45f;
            float actualHeight = smoothHeights[b] * maxBarHeight;

            // UNIVERSAL VARIABLES.
            float xPixelPos = startPosX + (b * barSpacing);
            float topY, bottomY, cornerRadius;
            ImU32 dynamicColor;

            if (visualMode == 0) {
                // MODE 0: CLASSIC BOTTOM ANCHOR (Colorful & Round)
                // 1. SHORTER HEIGHT (20% of screen max) so it isn't OVERWHELMING.
                float actualHeight = smoothHeights[b] * (viewportSize.y * 0.55f);
                if (actualHeight < viewportSize.y * 0.02f) actualHeight = viewportSize.y * 0.02f;

                bottomY = pillPosY - 30.0f; // Anchors exactly 30px above the UI Pill
                topY = bottomY - actualHeight;

                // 2. DYNAMIC EDM COLOR (Heatmap Effect):
                // Starts at 0.6 (Cool Blue/Cyan). As the bar grows, it pushes towards 0.0 (Bright Red).
                float hue = 0.6f - (smoothHeights[b] * 0.6f);
                if (hue < 0.0f) hue = 0.0f; // FAILSAFE to PREVENT NEGATIVE colors.

                // 100% Brightness and High Saturation for that neon club look
                ImU32 dynamicColor = ImColor::HSV(hue, 0.9f, 1.0f);

                ImGui::GetBackgroundDrawList()->AddRectFilled(
                    ImVec2(xPixelPos, topY),
                    ImVec2(xPixelPos + (barWidth * 0.8f), bottomY),
                    dynamicColor, 6.0f // Modern, subtle rounded corners
                );
                

            } else if (visualMode == 1) {
                // ==========================================
                // MODE 1: CENTER WAVEFORM (The "SKRILLEX FIX")
                // ==========================================

                // 1. THE DAMPENER: Multiply by 0.6f to COMPRESS loud DUBSTEP peaks
                float mode1Height = actualHeight * 0.6f;
                // 2. HARD CEILING: A failsafe so it NEVER grows taller than 75% of your screen.
                float maxSafeHeight = viewportSize.y * 0.75f;
                if (mode1Height > maxSafeHeight) mode1Height = maxSafeHeight;
                // 3. CENTER ANCHOR: Pin the bars precisely 45% up the screen
                float centerY = viewportSize.y * 0.45f;     // Anchored to middle of the screen.
                topY = centerY - (mode1Height * 0.5f);     // Grow UP from center.
                bottomY = centerY + (mode1Height * 0.5f);  // Grow DOWN from center.=
                // 4. DYNAMIC COLOR GRADIENT (Left to Right)
                // We divide the current bar 'b' by the total bars (64) to get a percentage.
                float barPercentage = static_cast<float>(b) / DISPLAY_BARS;
                // Start at 0.5 (Cyan) and smoothly shift towards 0.85 (Purple/Pink).
                float hue = 0.5f + (barPercentage * 0.35f);
                // 5. Reduce EYE STRAIN.
                // Brightness pulses with the music beat, but never goes fully dark
                float brightness = 0.5f + (smoothHeights[b] * 0.5f);

                // The final 0.85f drops the opacity to 85%, killing the blinding glare!
                ImGui::GetBackgroundDrawList()->AddRectFilled(
                    ImVec2(xPixelPos, topY), ImVec2(xPixelPos + barWidth, bottomY),
                    ImColor::HSV(hue, 0.8f, brightness, 0.85f), 2.0f
                );

            } else if (visualMode == 2) {
                // MODE 2: Neon Polyline Math.
                float actualHeight = smoothHeights[b] * (viewportSize.y * 0.35f);

                float peakY = centerY - actualHeight;
                float centerOfBarX = xPixelPos + (barWidth * 0.5f);
                
                mainLinePoints.push_back(ImVec2(centerOfBarX, peakY));
                shadowLinePoints.push_back(ImVec2(centerOfBarX, peakY + 6.0f));
            }

        } // END OF BAR DRAWING LOOP.

        // EXECUTE Polyline DRAW OUTSIDE the LOOP.
        if (visualMode == 2) {
            // ==========================================
            // HIGH-FIDELITY GPU SPLINE RASTERIZATION
            // ==========================================
            // Increase the vertex count by 10x to force the GPU to render a liquid-smooth curve!
            // This forces the GPU's rasterizer to actually wake up and do some heavy lifting.
            const int SPLINE_RESOLUTION = 10;

            mainLinePoints.push_back(ImVec2(startPosX + totalBarsWidth + 40.0f, centerY));
            shadowLinePoints.push_back(ImVec2(startPosX + totalBarsWidth + 40.0f, centerY + 6.0f));

            if (rawLinePoints.size() >= 2) {
                // Catmull-Rom Spline Math Lambda.
                auto CatmullRom = [](const ImVec2& p0, const ImVec2& p1, const ImVec2& p2, const ImVec2& p3, float t) {
                    float t2 = t * t;
                    float t3 = t2 * t;
                    return ImVec2(
                        0.5f * ((2.0f * p1.x) + (-p0.x + p2.x) * t + (2.0f * p0.x - 5.0f * p1.x + 4.0f * p2.x - p3.x) * t2 + (-p0.x + 3.0f * p1.x - 3.0f * p2.x + p3.x) * t3),
                        0.5f * ((2.0f * p1.y) + (-p0.y + p2.y) * t + (2.0f * p0.y - 5.0f * p1.y + 4.0f * p2.y - p3.y) * t2 + (-p0.y + 3.0f * p1.y - 3.0f * p2.y + p3.y) * t3)
                    );
                };

                // INTERPOLATE a smooth curve between every SINGLE audio peak.
                for (size_t i = 0; i < rawLinePoints.size() - 1; i++) {
                    ImVec2 p0 = (i == 0) ? rawLinePoints[i] : rawLinePoints[i - 1];
                    ImVec2 p1 = rawLinePoints[i];
                    ImVec2 p2 = rawLinePoints[i + 1];
                    ImVec2 p3 = (i + 2 < rawLinePoints.size()) ? rawLinePoints[i + 2] : p2;

                    // Generate high-density vertices for the GPU
                    for (int j = 0; j < SPLINE_RESOLUTION; j++) {
                        float t = static_cast<float>(j) / static_cast<float>(SPLINE_RESOLUTION);
                        ImVec2 interpolated = CatmullRom(p0, p1, p2, p3, t);
                        mainLinePoints.push_back(interpolated);
                        shadowLinePoints.push_back(ImVec2(interpolated.x, interpolated.y + 6.0f));
                    }
                }
                // PUSH the final point perfectly.
                mainLinePoints.push_back(rawLinePoints.back());
                shadowLinePoints.push_back(ImVec2(rawLinePoints.back().x, rawLinePoints.back().y + 6.0f));
            }

            ImGui::GetBackgroundDrawList()->AddPolyline(
                shadowLinePoints.data(), shadowLinePoints.size(),
                IM_COL32(230, 70, 230, 255), ImDrawFlags_None, 6.0f
            );

            ImGui::GetBackgroundDrawList()->AddPolyline(
                mainLinePoints.data(), mainLinePoints.size(),
                IM_COL32(70, 70, 230, 255), ImDrawFlags_None, 6.0f
            );
        }

        // ==========================================
        // IMGUI Phase 3: METADATA & UP NEXT QUEUE, IT'S NOW SEAMLESSLY. (UX FIX Step 1)
        // ==========================================
        std::string nowPlayingText;
        if (cleanTrackName.empty()) {
            nowPlayingText = "Now Playing: No Music Detected. Please SELECT A FOLDER first.";
        } else {
            nowPlayingText = "Now Playing: " + cleanTrackName;
        }

        // SAFE "UP NEXT" MATH (Prevents Modulo-by-Zero CRASHES).
        std::string upNextText = "Up Next: None";

        std::string nextTrackName = "None";
        if (playlist.size() > 0) {
            if (playbackMode == 3) {
                // MODE 3: SHUFFLE
                upNextText = "Up Next: Songs will be selected randomly (Shuffle Mode)";

            } else if (playbackMode == 2) {
                // MODE 2: REPEAT 1
                upNextText = "Up Next: Repeating Current Track";

            } else {
                // MODE 0 & 1: NORMAL & REPEAT ALL
                size_t nextIndex = (currentTrackIndex + 1) % playlist.size();
                std::string nextTrackName = fs::path(playlist[nextIndex]).filename().stem().string();
                upNextText = "Up Next: " + nextTrackName;
            }
        }

        // CENTERING math for the main title (NOW PLAYING).
        ImVec2 mainTextSize = ImGui::CalcTextSize(nowPlayingText.c_str());
        float mainTextPosX = (viewportSize.x - mainTextSize.x) * 0.5f;

        // CENTERING math for the the subtitle (UP NEXT).
        ImVec2 subTextSize = ImGui::CalcTextSize(upNextText.c_str());
        float subTextPosX = (viewportSize.x - subTextSize.x) * 0.5f;

        ImGui::SetNextWindowPos(ImVec2(0.0f, 45.0f));
        ImGui::SetNextWindowSize(ImVec2(viewportSize.x, 80.0f)); // Give the window enough height for two lines

        ImGui::Begin("Metadata", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoInputs);

        // 1. DRAW the "Now Playing" text (Bright/White Gray)
        ImGui::SetCursorPosX(mainTextPosX);
        ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "%s", nowPlayingText.c_str());

        // 2. DRAW the "Up Next" text (Dimmer, slightly transparent green/gray)
        ImGui::SetCursorPosX(subTextPosX);
        ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.7f, 0.8f), "%s", upNextText.c_str());

        ImGui::End();

        ImGui::SetNextWindowPos(ImVec2(pillPosX, pillPosY)); 
        ImGui::SetNextWindowSize(ImVec2(pillWidth, pillHeight));

        // Create a dark, rounded window container
        ImGui::Begin("Controls", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoBackground);

        // Center the buttons inside the pill
        ImGui::SetCursorPos(ImVec2(70.0f, 15.0f)); 
        // [C++ LEARNING] 'PushStyleVar' changes the internal ImGui drawing rules.
        // 'FrameRounding, 10.0f' curves the corners of every button drawn after this line!
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 10.0f);

        // ==================== PREV BUTTON (FIXED) ====================
        // // Triggers if clicked OR if '-' (main keyboard or numpad Subtract) is pressed.
        // FIXED: LEFT ARROW OR MINUS KEY OR NUMPAD MINUS
        bool pressedPrev = ImGui::IsKeyPressed(ImGuiKey_LeftArrow) || ImGui::IsKeyPressed(ImGuiKey_Minus) || ImGui::IsKeyPressed(ImGuiKey_KeypadSubtract);
        if (ImGui::Button("Prev", ImVec2(100, 50)) || (!io.WantCaptureKeyboard && pressedPrev)) {
            // THE 3 SECOND RULE: check how far into the song we are.
            // UX FIX Step 3: Crash prevention! Only seek/skip if a playlist actually exists.
            if (!playlist.empty()) {
                if (player.GetCurrentPosition() > 3.0f) {
                    player.Stop();
                    player.SeekToPosition(0.0f);
                    if (!isUserPaused) player.Play();
                } else {
                    // IF we are at the beginning, go to ACTUAL previous track.
                    player.Stop();
                    // Loops BACKWARDS cleanly EVEN if you are on Track 1.
                    currentTrackIndex = (currentTrackIndex - 1 + playlist.size()) % playlist.size();
                    selectedTrackPath = playlist[currentTrackIndex];
                    cleanTrackName = fs::path(selectedTrackPath).filename().stem().string();

                    player.Load(selectedTrackPath);
                    player.SetVolume(currentVolume);
                    trackDuration = player.GetDuration();
                    player.Play();
                    isUserPaused = false;
                }
            }
        }

        // [C++ LEARNING] Forces exactly 10 pixels of space between buttons
        ImGui::SameLine(0.0f, 10.0f);

        // ==================== STOP BUTTON (FIXED) ====================
        if (ImGui::Button("STOP", ImVec2(100, 50)) || (!io.WantCaptureKeyboard && ImGui::IsKeyPressed((ImGuiKey_S)))) {
            if (!playlist.empty()) {
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
        }

        ImGui::SameLine(0.0f, 10.0f); // C++ LEARNING WHILE CODE: Forces the next button to be on the SAME horizontal row!
        
        // ==================== Dynamic Play/Stop (Stop FIXED) Button ====================
        if (player.IsPlaying()) {
            if (ImGui::Button("PAUSE", ImVec2(100, 50)) || (!io.WantCaptureKeyboard && ImGui::IsKeyPressed((ImGuiKey_Space)))) {
                player.Stop();
                // [C++ LEARNING] Re-apply the volume state immediately so the new track doesn't blast at 100%!
                player.SetVolume(currentVolume);
                isUserPaused = true; // Tell the engine this was intentional!
            }
        } else {
            if (ImGui::Button("PLAY", ImVec2(100, 50)) || (!io.WantCaptureKeyboard && ImGui::IsKeyPressed((ImGuiKey_Space)))) {
                if (!playlist.empty()) {
                    player.Play();
                    // [C++ LEARNING] Re-apply the volume state immediately so the new track doesn't blast at 100%!
                    player.SetVolume(currentVolume);
                    isUserPaused = false; // Music is running naturally again
                }
            }
        }
        
        ImGui::SameLine(0.0f, 10.0f);

        // ==================== NEXT BUTTON ====================
        // Triggers if clicked OR if '+' (Equal key on main keyboard or Numpad Add) is pressed.
        // FIXED: RIGHT ARROW OR EQUAL/PLUS KEY OR NUMPAD PLUS
        bool pressedNext = ImGui::IsKeyPressed(ImGuiKey_RightArrow) || ImGui::IsKeyPressed(ImGuiKey_Equal) || ImGui::IsKeyPressed(ImGuiKey_KeypadAdd);
        if (ImGui::Button("Next", ImVec2(100, 50)) || (!io.WantCaptureKeyboard && pressedNext)) {
            // UX FIX Step 3: Crash prevention! Only skip if a playlist actually exists
            if (!playlist.empty()) {
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
        }

        ImGui::SameLine(0.0f, 10.0f);

        // ==================== DYNAMIC PLAYBACK MODE BUTTON ====================
        // THIS ARRAY holds the text for our 4 states.
        const char* modeLabels[] = { "Normal", "Repeat All", "Repeat 1", "Shuffle" };

        // THE BUTTON physically CHANGES its text based on the current playbackMode integer.
        if (ImGui::Button(modeLabels[playbackMode], ImVec2(100, 50))) {

            // [C++ NOTE] This smoothly cycles the number: 0 -> 1 -> 2 -> 3 -> 0 -> 1...
            playbackMode = (playbackMode + 1) % 4;
        }

        // ==================== SEEK BAR (NEW) ====================
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
        ImGui::PushItemWidth(540.0f);

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

        // PushItemWidth locks the slider's length to exactly 460 pixels 
        // so it perfectly matches the width of the 4 buttons above it!
        ImGui::PushItemWidth(380.0f);

        // SliderFloat min is 0.0f (mute), max is 2.0f (200% overdrive).
        if (ImGui::SliderFloat("##Volume", &currentVolume, 0.0f, 2.0f, "Volume: %.2fx")){
            // When the USER drags the slider, this BLOCKS TRIGGERS!
            player.SetVolume(currentVolume);
        }
        ImGui::PopItemWidth();

        ImGui::SameLine(0.0f, 10.0f);

        // The 100px Theme Switcher Button (430 + 10 + 100 = 540px grid perfectly maintained!).
        const char* themeLabels[] = { "Vis: Classic", "Vis: Real Waveform", "Vis: Neon Polyline" };
        if (ImGui::Button(themeLabels[visualMode], ImVec2(150, 24))) {
            visualMode = (visualMode + 1) % 3; // TOGGLES BETWEEN 0 and 1
        }

        ImGui::SameLine(0.0f, 10.0f);

        // TrumFaster Toggle Button (Dynamic Colors).
        if (isTrumFasterEnabled) {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.1f, 0.5f, 0.2f, 1.0f)); // GREEN WHEN ACTIVE.
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.2f, 0.6f, 0.3f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.0f, 0.4f, 0.1f, 1.0f));

            if (ImGui::Button("TrumFaster: ON", ImVec2(140, 24))) isTrumFasterEnabled = false; // TURN OFF.

            ImGui::PopStyleColor(3);
        } else {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.6f, 0.2f, 0.2f, 1.0f)); // RED when TURN OFF.
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.7f, 0.3f, 0.3f, 1.0f)); 
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.5f, 0.1f, 0.1f, 1.0f));

            if (ImGui::Button("TrumFaster: OFF", ImVec2(140, 24))) isTrumFasterEnabled = true; // TURN ON.

            ImGui::PopStyleColor(3);
        }

        // ==================== LIVE FOLDER LOADER (UPGRADED) ====================
        ImGui::SetCursorPos(ImVec2(70.0f, 145.0f)); // POSITION IT BELOW THE VOLUME SLIDER

        // [C++ Coding-while-Learning] A static char array holds the text the user types into ImGui.
        // UI FIX Step 4: START with an EMPTY buffer.
        static char folderPathBuffer[256] = "";

        // ---------------------------------------------------------
        // BACKGROUND THREAD VARIABLES (Survives frame resets)
        // ---------------------------------------------------------
        static std::atomic<bool> isBrowsing = false;
        static std::string asyncSelectedPath = "";

        // If the background thread finished finding a folder, copy it to the UI safely.
        if (!isBrowsing && !asyncSelectedPath.empty()) {
            strncpy(folderPathBuffer, asyncSelectedPath.c_str(), sizeof(folderPathBuffer) - 1);
            folderPathBuffer[sizeof(folderPathBuffer) - 1] = '\0'; // SAFETY NULL-TERMINATION.
            asyncSelectedPath.clear(); // CLEAR it so it only UPDATES once.
        }

        // ---------------------------------------------------------
        // NEW FEATURE: Native Mac "Browse" Button
        // ---------------------------------------------------------
        if (isBrowsing) {
            // [UI TRICK] DISABLED the button while the window is open so they can't spam 50 windows.
            ImGui::BeginDisabled();
            ImGui::Button("Browsing...", ImVec2(100, 24));
            ImGui::EndDisabled();
        } else {
            if (ImGui::Button("Browse...", ImVec2(100, 24))) {
                isBrowsing = true; // LOCK the BUTTON.
                asyncSelectedPath = ""; // CLEAR old paths

                // [NOTE] SPAWN a background worker to open the Mac Finder window.
                std::thread([]() {

                    // ------ CROSS-PLATFORM DIALOG GENERATOR ------
                    std::string command;
                    
                    #ifdef _WIN32
                        // Windows uses PowerShell to OPEN the File Explorer dialog.
                        command = "powershell -NoProfile -Command \"(New-Object -ComObject Shell.Application).BrowseForFolder(0, 'Select Music Folder', 0).Self.Path\"";
                        #define POPEN _popen
                        #define PCLOSE _pclose
                    #elif __APPLE__
                        // macOS uses AppleScript.
                        command = "osascript -e 'POSIX path of (choose folder with prompt \"Select Music Folder\")'";
                        #define POPEN popen
                        #define PCLOSE pclose
                    #elif __linux__
                        // Linux uses Zenity
                        command = "zenity --file-selection --directory --title=\"Select Music Folder\"";
                        #define POPEN popen
                        #define PCLOSE pclose
                    #endif

                    FILE* pipe = POPEN(command.c_str(), "r");
                    if (pipe) {
                        char pathBuffer[512];
                        if (fgets(pathBuffer, sizeof(pathBuffer), pipe) != nullptr) {
                            // STRIP BOTH standard UNIX newlines and Windows carriage returns (\r\n)
                            pathBuffer[strcspn(pathBuffer, "\r\n")] = 0;
                            asyncSelectedPath = pathBuffer; // SAVE the result.
                        }
                        pclose(pipe);
                    }
                    isBrowsing = false; // UNLOCK the button when the Mac window closes
                }).detach(); // .detach() tells the main engine: "Don't wait for me, keep running!"
            }
        }

        ImGui::SameLine(0.0f, 10.0f);

        ImGui::PushItemWidth(300.0f);
        // THIS creates a text box where I can type any Mac folder path!
        // UX FIX Step 5: Beautiful placeholder text guiding the user!
        ImGui::InputTextWithHint("##FolderPath", "Select a music directory...", folderPathBuffer, sizeof(folderPathBuffer));
        ImGui::PopItemWidth();

        ImGui::SameLine(0.0f, 10.0f);
        
        // When the user clicks the button, the engine re-scans the COMPUTER.
        if (ImGui::Button("Load Folder", ImVec2(120, 0))) {
            std::string newPath = folderPathBuffer;

            // UX FIX Step 6: Smart Error Handling!
            if (newPath.empty()) {
                std::cout << "[ENGINE] Please select or type a folder path first!\n";
            }
            else if (fs::exists(newPath) && fs::is_directory(newPath)) {
                std::cout << "[ENGINE] Scanning new folder: " << newPath << "\n";
                std::vector<std::string> tempPlaylist;

                for (const auto &entry : fs::directory_iterator(newPath)) {
                    if (entry.path().extension() == ".mp3" || entry.path().extension() == ".MP3" || entry.path().extension() == ".wav" || entry.path().extension() == ".WAV" || entry.path().extension() == ".flac" || entry.path().extension() == ".FLAC"){
                        tempPlaylist.push_back(entry.path().string());
                    }
                }

                std::sort(tempPlaylist.begin(), tempPlaylist.end());

                if (!tempPlaylist.empty()) {
                    playlist = tempPlaylist;

                    player.Stop();
                    currentTrackIndex = 0;
                    selectedTrackPath = playlist[currentTrackIndex];
                    cleanTrackName = fs::path(selectedTrackPath).filename().stem().string();

                    player.Load(selectedTrackPath);
                    player.SetVolume(currentVolume);
                    trackDuration = player.GetDuration();
                    player.Play(); 

                    isUserPaused = false;
                    for (size_t i = 0; i < frozenFrequencies.size(); i++) {
                        frozenFrequencies[i] = 0.0f;
                    }
                } else {
                    std::cout << "[ ⚠ ENGINE WARNING!] No Audio files found in the FOLDER!\n";
                }
            } else {
                // Now it tells you EXACTLY what path failed!
                std::cout << "[ ⚠ ENGINE WARNING!] The folder path is INVALID: '" << newPath << "'\n";
            }
        }

        ImGui::PopStyleVar();
        ImGui::End();

        // ==========================================
        // IMGUI Phase 4: RENDER TO SCREEN
        // ==========================================
        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        // Swap the video buffers and push the pixel data to your monitor
        window.Update();

        // Let TrumFaster calculate the exact sleep math instead of hardcoding it.
        trumFaster.EndFrame();
    }

    // ==========================================
    // IMGUI Phase 5: CLEAN SHUTDOWN
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