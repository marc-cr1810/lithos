cmake_minimum_required(VERSION 3.16)
project(lithos VERSION 1.0.0 LANGUAGES CXX C)

# MSVC Warning Suppressions
if(MSVC)
    add_compile_definitions(_SILENCE_STDEXT_ARR_ITERS_DEPRECATION_WARNING)
    add_compile_definitions(_SILENCE_ALL_MS_EXT_DEPRECATION_WARNINGS)

    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
        add_compile_options(/Ob1)
        add_compile_definitions(_ITERATOR_DEBUG_LEVEL=0)
    endif()
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(UNIX AND NOT APPLE)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -rdynamic")
endif()

include(FetchContent)

# Dependencies

# OpenGL
find_package(OpenGL REQUIRED)

# GLFW
find_package(glfw3 QUIET)
if(NOT glfw3_FOUND)
    message(STATUS "GLFW3 not found, fetching...")
    
    # Disable GLFW docs, tests, examples to avoid Doxygen/Nuklear warnings
    set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
    set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    
    FetchContent_Declare(
        glfw
        GIT_REPOSITORY https://github.com/glfw/glfw.git
        GIT_TAG 3.3.8
    )
    FetchContent_MakeAvailable(glfw)
else()
    set(GLFW_LIBRARIES glfw)
endif()

# GLM
find_package(glm QUIET)
if(NOT glm_FOUND)
    message(STATUS "GLM not found, fetching...")
    FetchContent_Declare(
        glm
        GIT_REPOSITORY https://github.com/g-truc/glm.git
        GIT_TAG 0.9.9.8
    )
    FetchContent_MakeAvailable(glm)
endif()

# Argparse
FetchContent_Declare(
  argparse
  GIT_REPOSITORY https://github.com/p-ranav/argparse.git
  GIT_TAG v3.0
)
FetchContent_MakeAvailable(argparse)

# EnTT
FetchContent_Declare(
    entt
    GIT_REPOSITORY https://github.com/skypjack/entt.git
    GIT_TAG v3.13.0
)
FetchContent_MakeAvailable(entt)

# GLEW
find_package(GLEW QUIET)
if (NOT GLEW_FOUND)
    message(STATUS "GLEW not found, fetching release tarball...")
    FetchContent_Declare(
       glew
       URL https://github.com/nigels-com/glew/releases/download/glew-2.2.0/glew-2.2.0.tgz
       DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    )
    
    # We need to manually populate because the CMakeLists.txt is in build/cmake
    FetchContent_GetProperties(glew)
    if(NOT glew_POPULATED)
        FetchContent_Populate(glew)
        # The cmake file is in build/cmake
        add_subdirectory(${glew_SOURCE_DIR}/build/cmake ${glew_BINARY_DIR})
    endif()
    
    # Official GLEW cmake config defines 'glew_s' for static lib.
    set(GLEW_TARGET glew_s)
    include_directories(${glew_SOURCE_DIR}/include)
else()
    set(GLEW_TARGET GLEW::GLEW)
endif()

# nlohmann_json
FetchContent_Declare(
    json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG v3.11.2
)
FetchContent_MakeAvailable(json)

# FastNoise2
FetchContent_Declare(
    FastNoise2
    GIT_REPOSITORY https://github.com/Auburn/FastNoise2.git
    GIT_TAG v0.10.0-alpha
)
FetchContent_MakeAvailable(FastNoise2)

# ImGui
FetchContent_Declare(
    imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG docking
)
FetchContent_MakeAvailable(imgui)

# spdlog
FetchContent_Declare(
    spdlog
    GIT_REPOSITORY https://github.com/gabime/spdlog.git
    GIT_TAG v1.12.0
)
FetchContent_MakeAvailable(spdlog)

# Debug Macro
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    add_definitions(-DLITHOS_DEBUG)
endif()



# MSVC Optimizations for Release
if(MSVC AND CMAKE_BUILD_TYPE STREQUAL "Release")
    add_compile_options(/O2 /Ob2 /Oi /Ot /Oy /GL)
    add_link_options(/LTCG)
endif()

add_executable(lithos 
    src/main.cpp 
    src/core/StateManager.cpp
    src/core/Application.cpp
    src/core/ResourceManager.cpp
    src/states/LoadingState.cpp
    src/states/GameState.cpp
    src/states/MenuState.cpp
    src/render/Shader.cpp 
    src/render/Camera.cpp
    src/render/Texture.cpp
    src/render/TextureAtlas.cpp
    src/render/Framebuffer.cpp
    src/render/ModelLoader.cpp
    src/render/NoisePreview.cpp
    src/world/Chunk.cpp
    src/world/WorldGenerator.cpp
    src/world/CaveGenerator.cpp
    src/world/TreeDecorator.cpp
    src/world/FloraDecorator.cpp
    src/world/OreDecorator.cpp
    src/world/World.cpp
    src/world/blocks/LiquidBlock.cpp
    src/world/BlockRegistry.cpp
    src/world/gen/NoiseManager.cpp
    src/world/gen/Landform.cpp
    src/world/gen/RockStrata.cpp
    src/world/gen/BlockLayerConfig.cpp
    src/ecs/Systems.cpp
    src/debug/Profiler.cpp
    src/debug/CrashHandler.cpp
    src/debug/Logger.cpp
    src/debug/Benchmark.cpp
    
    # ImGui
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_demo.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp
)

target_include_directories(lithos PRIVATE 
    ${CMAKE_SOURCE_DIR}/src
    ${imgui_SOURCE_DIR} 
    ${imgui_SOURCE_DIR}/backends
)

target_link_libraries(lithos PRIVATE 
    OpenGL::GL
    glfw
    ${GLEW_TARGET}
    EnTT::EnTT
    nlohmann_json::nlohmann_json
    spdlog::spdlog
    argparse::argparse
    FastNoise
)

if(WIN32)
    target_link_libraries(lithos PRIVATE Dbghelp)
endif()

target_compile_definitions(lithos PRIVATE USE_GLEW GLEW_NO_GLU)


if (TARGET glm::glm)
    target_link_libraries(lithos PRIVATE glm::glm)
else()
    target_include_directories(lithos PRIVATE ${glm_SOURCE_DIR})
endif()
