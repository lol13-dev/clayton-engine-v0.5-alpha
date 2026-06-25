cmake_minimum_required(VERSION 3.15)
project(ClaytonEngine VERSION 0.9.0)

# ENFORCE C++17 Standard.
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# -- NEW -- OUTPUT the .app or .exe directly to the root ClaytonEngine folder.
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}")

# 1. GATHER all my Engine Source Files.
file(GLOB_RECURSE ENGINE_SOURCES "src/*.cpp")

# 2. GATHER all ImGui SOURCE FILES.
set(IMGUI_DIR "third_party/imgui")
file(GLOB IMGUI_SOURCES
    "${IMGUI_DIR}/*.cpp"
    "${IMGUI_DIR}/backends/imgui_impl_glfw.cpp"
    "${IMGUI_DIR}/backends/imgui_impl_opengl3.cpp"
)

# 3. CREATE the executable. (Added WIN32 and MACOSX_BUNDLE back to generate standalone apps!)
add_executable(ClaytonEngine WIN32 MACOSX_BUNDLE ${ENGINE_SOURCES} ${IMGUI_SOURCES})

# 4. TELL the compiler where my HEADER files (.h) LIVE.
target_include_directories(ClaytonEngine PRIVATE
    "src"
    "third_party"
    "third_party/glfw/include"
    "third_party/glm"
    "${IMGUI_DIR}"
    "${IMGUI_DIR}/backends"
)

# 5. FIND OpenGL NATIVELY on ANY OPERATING SYSTEM.
find_package(OpenGL REQUIRED)

# 6. SILENCE the compiler WARNING.
add_compile_definitions(GL_SILENCE_DEPRECATION)

# 7. Cross-platform Linking.
if(APPLE)
    # macOS: LINK Mac Frameworks and my existing Mac GLFW library (ADDED ${CMAKE_SOURCE_DIR}/)
    target_link_libraries(ClaytonEngine PRIVATE OpenGL::GL "${CMAKE_SOURCE_DIR}/third_party/glfw/lib/libglfw3.a" "-framework Cocoa" "-framework IOKit" "-framework CoreVideo")
    message(STATUS "CONFIGURING Clayton Engine for macOS")

elseif(WIN32)
    # Windows: LINK Windows OpenGL and Windows GLFW library (.lib) (ADDED ${CMAKE_SOURCE_DIR}/)
    target_link_libraries(ClaytonEngine PRIVATE opengl32 "${CMAKE_SOURCE_DIR}/third_party/glfw/lib/glfw3.lib")
    message(STATUS "CONFIGURING Clayton Engine for Windows")

elseif(UNIX AND NOT APPLE)
    # Linux: LINK X11/Wayland LIBRARIES and Linux GLFW library (.so)
    target_link_libraries(ClaytonEngine PRIVATE OpenGL::GL glfw m dl pthread)
    message(STATUS "CONFIGURING Clayton Engine for Linux")
endif()