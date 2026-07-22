cmake_minimum_required(VERSION 4.3)
project(dusk)

set(CMAKE_CXX_STANDARD 23)

find_package(SFML 3 COMPONENTS Graphics Window System REQUIRED)

add_executable(dusk src/main.cpp
        include/tools/camera.h++
        include/tools/camera_controller.h++
        include/math/Mat4.h++
        include/math/Vec2.h++
        include/math/Vec3.h++
        include/objects/cube.h++
        include/objects/star.h++
        include/world/starfield.h++
        include/rendering/cube_renderer.h++
        include/rendering/projector.h++
        include/rendering/star_renderer.h++)

target_include_directories(dusk PRIVATE include)
target_include_directories(dusk PRIVATE src)

target_link_libraries(dusk PRIVATE
        SFML::Graphics
        SFML::Window
        SFML::System
)
