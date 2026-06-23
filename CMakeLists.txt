cmake_minimum_required(VERSION 3.20)
project(Behest VERSION 1.0.0 LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Vulkan REQUIRED)
find_package(glfw3 REQUIRED)
find_package(glm REQUIRED)

add_executable(behest
    src/main.cpp
    src/Engine.cpp
    src/Renderer.cpp
    src/Texture.cpp
    src/Player.cpp
    src/Npc.cpp
    src/Map.cpp
    src/MapManager.cpp
    src/Font.cpp
    src/TmxLoader.cpp
)

target_include_directories(behest PRIVATE
    ${Vulkan_INCLUDE_DIRS}
    ${GLFW_INCLUDE_DIRS}
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/third_party
)

target_link_libraries(behest
    ${Vulkan_LIBRARIES}
    glfw
    glm::glm
    z
)

target_compile_options(behest PRIVATE -Wall -Wextra -Wno-unused-parameter)
