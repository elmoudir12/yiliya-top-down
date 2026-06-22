cmake_minimum_required(VERSION 3.20)
project(YiliyaTopDown VERSION 1.0.0 LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Vulkan REQUIRED)
find_package(glfw3 REQUIRED)
find_package(glm REQUIRED)

add_executable(yiliya_game
    src/main.cpp
    src/Engine.cpp
    src/Renderer.cpp
    src/Texture.cpp
    src/Player.cpp
    src/Map.cpp
    src/MapManager.cpp
)

target_include_directories(yiliya_game PRIVATE
    ${Vulkan_INCLUDE_DIRS}
    ${GLFW_INCLUDE_DIRS}
    ${CMAKE_SOURCE_DIR}
)

target_link_libraries(yiliya_game
    ${Vulkan_LIBRARIES}
    glfw
    glm::glm
    z
)

target_compile_options(yiliya_game PRIVATE -Wall -Wextra -Wno-unused-parameter)
