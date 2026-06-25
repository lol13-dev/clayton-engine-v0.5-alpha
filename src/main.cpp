// main.cpp
// Include my engine header file
#include "core/Engine.h"
#include <iostream>
#include <filesystem>

int main(int argc, char** argv) {
    // ==========================================
    // MAC .APP BUNDLE WORKING DIRECTORY FIX
    // ==========================================
    // This FORCES the .app bundle to look exactly where it is located on our HARD DRIVE.
    if (argc > 0) {
        std::filesystem::path exePath(argv[0]);
        std::filesystem::path exeDir = std::filesystem::absolute(exePath).parent_path();

        #ifdef __APPLE__
            // If running from inside a Mac .app bundle (Contents/MacOS), go up 3 folders to hit the root!
            if (exeDir.string().find("Contents/MacOS") != std::string::npos) {
                std::filesystem::current_path(exeDir.parent_path().parent_path().parent_path());
            } else {
                std::filesystem::current_path(exeDir);
            }
        #else
            std::filesystem::current_path(exeDir);
        #endif

        std::cout << "[SYSTEM] Working directory set to: " << std::filesystem::current_path() << "\n";
    }


    // Create an Engine objects.
    Engine engine;

    // START the engine.
    engine.Initialize();

    // RUN the engine.
    engine.Run();

    // SHUTDOWN the engine.
    engine.Shutdown();

    // return 0; = SUCCESSFUL PROGRAM EXIT.
    return 0;
}