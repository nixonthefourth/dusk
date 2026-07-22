cmake_minimum_required(VERSION 4.3)
project(dusk)

set(CMAKE_CXX_STANDARD 23)

find_package(SFML 3 COMPONENTS Graphics Window System REQUIRED)

add_executable(dusk src/main.cpp
        include/camera.h++)

target_include_directories(dusk PRIVATE include)
target_include_directories(dusk PRIVATE src)

target_link_libraries(dusk PRIVATE
        SFML::Graphics
        SFML::Window
        SFML::System
)