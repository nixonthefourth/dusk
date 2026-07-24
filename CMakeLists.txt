cmake_minimum_required(VERSION 4.3)
project(dusk)

set(CMAKE_CXX_STANDARD 23)

find_package(SFML 3 COMPONENTS Graphics Window System REQUIRED)

add_executable(dusk src/main.cpp
        include/tools/camera.h++
        include/tools/camera_controller.h++
        include/tools/ship_controller.h++
        include/io/obj_loader.h++
        include/math/Mat4.h++
        include/math/Vec2.h++
        include/math/Vec3.h++
        include/model/vector_model.h++
        include/objects/cube.h++
        include/objects/ship.h++
        include/objects/star.h++
        include/scenes/default_scene.h++
        include/scenes/scene.h++
        include/scenes/scene_manager.h++
        include/systems/ship_physics.h++
        include/world/starfield.h++
        include/world/world.h++
        include/rendering/cube_renderer.h++
        include/rendering/hud_renderer.h++
        include/rendering/projector.h++
        include/rendering/ship_renderer.h++
        include/rendering/star_renderer.h++)

target_include_directories(dusk PRIVATE include)
target_include_directories(dusk PRIVATE src)

target_link_libraries(dusk PRIVATE
        SFML::Graphics
        SFML::Window
        SFML::System
)
