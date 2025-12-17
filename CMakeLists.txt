cmake_minimum_required(VERSION 3.16)
project(minceraft VERSION 1.0.0 LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)

# Dependencies

# OpenGL
find_package(OpenGL REQUIRED)

# GLFW
find_package(glfw3 QUIET)
if(NOT glfw3_FOUND)
    message(STATUS "GLFW3 not found, fetching...")
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

# GLEW
find_package(GLEW QUIET)
if (NOT GLEW_FOUND)
    message(STATUS "GLEW not found, fetching release tarball...")
    FetchContent_Declare(
       glew
       URL https://github.com/nigels-com/glew/releases/download/glew-2.2.0/glew-2.2.0.tgz
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

add_executable(minceraft 
    src/main.cpp 
    src/render/Shader.cpp 
    src/render/Camera.cpp
    src/render/Texture.cpp
    src/render/TextureAtlas.cpp
    src/world/Chunk.cpp
    src/world/Player.cpp
    src/world/WorldGenerator.cpp
    src/world/World.cpp
)

target_link_libraries(minceraft PRIVATE 
    OpenGL::GL
    glfw
    ${GLEW_TARGET}
)

target_compile_definitions(minceraft PRIVATE USE_GLEW GLEW_NO_GLU)


if (TARGET glm::glm)
    target_link_libraries(minceraft PRIVATE glm::glm)
else()
    target_include_directories(minceraft PRIVATE ${glm_SOURCE_DIR})
endif()
