# dusk

![Flight Demo](demo/demo.gif)

`dusk` is a small SFML/C++ experiment for faking 3D with 2D rendering. It keeps a simple 3D world in memory, transforms points through a camera view matrix, projects those points onto the 2D screen, and draws the result with SFML primitives.

The current project includes:

- A working main menu with keyboard and mouse start input.
- A camera that follows a player ship.
- A wireframe Sidewinder-inspired ship rendered from vector line data.
- A first test scene with a planet and a rotating cube portal.
- A second test scene with two projected planets.
- Cube-triggered scene teleportation.
- A recycled endless starfield.
- Frustum clipping for projected points and lines.
- Newtonian-ish ship physics with persistent velocity.
- A minimalist HUD drawn without fonts.

## Build

Requirements:

- CMake 4.3 or newer, matching the current `CMakeLists.txt`.
- C++23 compiler.
- SFML 3 with Graphics, Window, and System components.

Configure and build:

```sh
cmake -S . -B build
cmake --build build
```

Run:

```sh
./build/dusk
```

## Controls

The menu accepts:

- `Enter`: start the first test scene.
- `Space`: start the first test scene.
- Mouse click on `PLAY`: start the first test scene.
- `Escape`: quit.

Playable scenes use a ship-first input pipeline:

```text
keyboard -> ship controls -> ship physics -> camera follows ship -> render
```

Flight controls:

- `W`: increase throttle.
- `S`: decrease throttle.
- `Arrow Down`: toggle forward/reverse thrust and reset throttle to `0`.
- `A`: yaw/turn ship left.
- `D`: yaw/turn ship right.
- `Q`: pitch nose up.
- `E`: pitch nose down.
- `Arrow Up`: hold to orbit the camera around the ship.
- `Escape`: quit.

In the first test scene, fly the ship through the rotating cube to teleport into the second scene with two planets.

Because movement is velocity-based, reducing throttle does not stop the ship immediately. It only stops adding acceleration. To brake:

1. Tap `Arrow Down` to switch to reverse thrust.
2. Hold `W` to add reverse thrust.
3. Lower throttle with `S` or toggle direction once you are near a stop.

## Project Layout

```text
src/
  main.cpp

include/
  io/
    obj_loader.h++

  math/
    Vec2.h++
    Vec3.h++
    Mat4.h++

  model/
    vector_model.h++

  objects/
    ship.h++
    cube.h++
    planet.h++
    star.h++

  scenes/
    scene.h++
    default_scene.h++
    first_test_scene.h++
    main_menu.h++
    scene_manager.h++
    two_planet_scene.h++

  systems/
    ship_physics.h++

  tools/
    camera.h++
    camera_controller.h++
    ship_controller.h++

  world/
    world.h++
    starfield.h++

  rendering/
    projector.h++
    star_renderer.h++
    planet_renderer.h++
    cube_renderer.h++
    ship_renderer.h++
    hud_renderer.h++
```

The important separation is:

- `objects/`: data and local object helpers.
- `model/`: shared wire/vector model structures.
- `io/`: file loading and conversion helpers, such as OBJ-to-vector conversion.
- `scenes/`: scene interface, active scene ownership, and scene presets.
- `systems/`: simulation logic, such as physics.
- `tools/`: input/control helpers and camera behavior.
- `world/`: ownership of world-coordinate objects.
- `rendering/`: code that draws, projects, clips, or turns state into pixels.
- `src/main.cpp`: orchestration only.

That last point matters. `main.cpp` should ideally stay boring:

```cpp
if (sceneManager.activeScene().acceptsShipInput())
    updateShipFromKeyboard(world.playerShip, dt, shipInputState);

sceneManager.activeScene().updatePhysics(dt);
sceneManager.activeScene().updateCamera(camera, dt, shipCameraRig);
sceneManager.activeScene().updateStreaming(camera);

starRenderer.draw(window, world.starfield.stars(), camera);
planetRenderer.draw(window, world.planets, camera);

if (world.cubeActive)
    cubeRenderer.draw(window, world.cube, camera);

shipRenderer.draw(window, world.playerShip, camera);

if (sceneManager.activeScene().showsHud())
    hudRenderer.draw(window, world.playerShip);

sceneManager.activeScene().drawOverlay(window);
```

This keeps input, physics, camera, world streaming, and rendering from tangling together.

## The World Coordinate Model

Everything exists in world coordinates as `Vec3` values:

```cpp
struct Vec3 {
    float x = 0.f, y = 0.f, z = 0.f;
};
```

The convention used throughout the project is:

- `+x`: right.
- `+y`: up.
- `+z`: forward.

The player acts on one singular ship object in the active scene's world:

```cpp
struct World {
    Starfield starfield;
    Cube cube;
    bool cubeActive = true;
    std::vector<Planet> planets;
    Ship playerShip;
};
```

The ship is just another object in the world. The camera follows it, but the ship does not "belong" to the camera.

That means the pipeline is:

```text
Ship has world position and velocity.
Camera chooses a world position behind/up from the ship.
Renderer transforms world positions into camera space.
Projector turns camera space into screen pixels.
```

`main.cpp` does not construct world objects directly anymore. It asks the scene manager for the active scene:

```cpp
SceneManager sceneManager;
sceneManager.setScene<MainMenuScene>();
World& world = sceneManager.world();
```

That active scene owns the world, and the world owns objects in world coordinates.

## How Fake 3D Rendering Works

The project does not use OpenGL, a depth buffer, or real 3D meshes. It keeps 3D coordinates and manually projects them onto a 2D SFML window.

The core type is `Projector` in:

```text
include/rendering/projector.h++
```

Projection happens in two broad steps:

1. Convert a point from world space to camera space.
2. Convert camera space to screen space.

### World Space To Camera Space

World space is the shared coordinate system where objects live.

Camera space is the coordinate system from the camera's point of view:

- Camera is at the origin.
- `+z` means in front of the camera.
- `+x` means right on screen.
- `+y` means up on screen.

The camera builds a view matrix:

```cpp
Mat4 createViewMatrix(const Camera& camera) const;
```

This matrix is built from an orthonormal basis:

```text
right   = camera's local right direction
up      = camera's local up direction
forward = camera's local forward direction
```

Then a world point is transformed:

```cpp
Vec3 cameraSpace = transformPoint(viewMatrix, worldPosition);
```

The view matrix also subtracts camera position, so points are relative to the camera.

### Camera Space To Screen Space

Once a point is in camera space, perspective projection is simple:

```text
screenX = centerX + cameraX * focalLength / cameraZ
screenY = centerY - cameraY * focalLength / cameraZ
```

`cameraZ` is depth. As depth grows, the division makes things appear smaller and closer to the center.

The focal length comes from the vertical field of view:

```text
focalLength = viewportHeight * 0.5 / tan(fov * 0.5)
```

So a narrow FOV gives a larger focal length, which feels more zoomed in. A wide FOV gives a smaller focal length, which feels more zoomed out.

### Frustum Clipping

The frustum is the visible 3D cone/pyramid in front of the camera. The project stores it as:

```cpp
struct Frustum {
    float nearPlane;
    float farPlane;
    float halfVerticalFovTan;
    float aspect;
};
```

For points, clipping checks:

- Is `z` between near and far?
- Is `x` inside the horizontal FOV at this depth?
- Is `y` inside the vertical FOV at this depth?

For lines, such as cube edges and ship edges, the renderer clips the line segment against the frustum before projection. This prevents nasty projection artifacts when part of a line is behind the camera or outside the view.

## The Ship

The ship state lives in:

```text
include/objects/ship.h++
```

Important fields:

```cpp
struct Ship {
    Vec3 position;
    Vec3 velocity;

    float yaw;
    float pitch;

    float throttle;
    bool reverseThrust;

    float maxAcceleration;
    float throttleChangeSpeed;
    float yawSpeed;
    float pitchSpeed;
};
```

The ship has orientation helpers:

```cpp
Vec3 shipForward(const Ship& ship);
Vec3 shipRight(const Ship& ship);
Vec3 shipUp(const Ship& ship);
Vec3 shipLocalToWorld(const Ship& ship, const Vec3& local);
```

These let the renderer and physics agree about which way the ship is pointing.

## Newtonian Ship Physics

Physics lives in:

```text
include/systems/ship_physics.h++
```

The key function is:

```cpp
inline void integrateShipPhysics(Ship& ship, float dt)
{
    const float thrustDirection = ship.reverseThrust ? -1.f : 1.f;
    const Vec3 acceleration =
        shipForward(ship) *
        ship.maxAcceleration *
        ship.throttle *
        thrustDirection;

    ship.velocity += acceleration * dt;
    ship.position += ship.velocity * dt;
}
```

This is basic Euler integration:

```text
velocity = velocity + acceleration * dt
position = position + velocity * dt
```

There is intentionally no drag right now. If the ship gains velocity, it keeps that velocity until thrust changes it.

### Why W And S Do Not Move Directly

`W` and `S` change throttle, not position:

```cpp
if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::W))
    ship.throttle += ship.throttleChangeSpeed * dt;

if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::S))
    ship.throttle -= ship.throttleChangeSpeed * dt;
```

The physics system applies that throttle as acceleration. This is what makes the ship drift.

### Reverse Thrust

`Arrow Down` toggles reverse thrust:

```cpp
ship.reverseThrust = !ship.reverseThrust;
ship.throttle = 0.f;
```

The throttle reset makes the mode switch predictable:

- Forward mode: throttle accelerates along ship forward.
- Reverse mode: throttle accelerates opposite ship forward.

## The Camera

Camera state lives in:

```text
include/tools/camera.h++
```

Ship camera behavior lives in:

```text
include/tools/ship_controller.h++
```

The camera follows the ship:

```cpp
updateShipCamera(camera, world.playerShip, dt, shipCameraRig);
```

Normal mode:

- Camera sits behind the ship along `-shipForward`.
- Camera sits above the ship using world up.
- Camera looks at a point ahead of the ship.

Showcase mode:

- Hold `Arrow Up`.
- Camera orbits around the ship.
- Useful for inspecting the wireframe model.

## The Starfield

The starfield lives in:

```text
include/world/starfield.h++
```

It does not create infinite stars. Instead, it keeps a fixed pool:

```cpp
int starCount = 3000;
```

Those stars are randomly distributed in a large volume around the current starfield center:

```cpp
float radius = 30000.f;
```

When the camera moves far enough from that center, the pool is recycled around the new camera area:

```cpp
float recycleThreshold = 0.5f;
```

This gives the illusion of endless space without rendering millions of stars.

## The Ship Renderer

The ship renderer lives in:

```text
include/rendering/ship_renderer.h++
```

The ship is a vector line model:

```cpp
struct VectorModel {
    std::vector<Vec3> vertices;
    std::vector<VectorLine> lines;
};
```

Each line indexes two vertices:

```cpp
struct VectorLine {
    int start = 0;
    int end = 0;
    bool hideWhenViewedFromAbove = false;
};
```

That `hideWhenViewedFromAbove` flag is used for underside lines, so the ship can remain wireframe without always showing its bottom structure through the top.

The model is owned by the ship object itself:

```cpp
struct Ship {
    Vec3 position;
    Vec3 velocity;
    float throttle;
    VectorModel model = createDefaultShipModel();
};
```

That means the object carries its physics state, gameplay parameters, and renderable vector model together. The renderer only reads `ship.model`.

The draw path is:

```text
local ship vertex
  -> shipLocalToWorld()
  -> camera view matrix
  -> frustum line clipping
  -> projection
  -> SFML line draw
```

## OBJ Loading

OBJ loading lives in:

```text
include/io/obj_loader.h++
```

The loader converts OBJ data into the same `VectorModel` format used by the ship renderer:

```cpp
std::optional<VectorModel> loadObjFileAsVectorModel(
    const std::string& path,
    const ObjLoadOptions& options = {}
);
```

Supported OBJ records:

- `v x y z`: loaded as model vertices.
- `f ...`: face boundaries become unique wire edges.
- `l ...`: line records become wire edges.

Ignored OBJ data:

- normals
- UVs
- materials
- smoothing groups
- filled triangles/polygons

This is intentional: `dusk` is a line-rendered fake-3D experiment, so the useful conversion is:

```text
OBJ vertices and faces -> unique vertices and edges -> VectorModel
```

### Loading An OBJ Into The Ship

The ship object exposes:

```cpp
bool loadObjModel(const std::string& path, const ObjLoadOptions& options = {});
```

Example:

```cpp
ObjLoadOptions options;
options.scale = 100.f;
options.flipZ = true;
world.playerShip.loadObjModel("assets/ships/my_ship.obj", options);
```

Where to put that? The recommended place is inside your scene header, because the scene decides which world objects exist and how they are configured.

For example, in `include/scenes/default_scene.h++`:

```cpp
DefaultScene()
{
    world_.cube.position = {0.f, 0.f, 4000.f};

    ObjLoadOptions options;
    options.scale = 100.f;
    world_.playerShip.loadObjModel("assets/ships/my_ship.obj", options);
}
```

If loading fails, the ship keeps its built-in vector model.

### OBJ Index Notes

OBJ indices are 1-based:

```obj
v 0 0 0
v 1 0 0
l 1 2
```

The loader converts those to zero-based C++ indices.

It also accepts slash syntax:

```obj
f 1/1/1 2/2/1 3/3/1
```

Only the vertex index before the first slash is used.

Negative OBJ indices are supported too, because many simple exporters use them:

```obj
f -4 -3 -2 -1
```

### Hand-Authored Models Vs OBJ Models

Hand-authored vector models can use `hideWhenViewedFromAbove` per edge:

```cpp
{0, 6, true}
```

OBJ files do not contain that project-specific visibility hint, so all OBJ edges load with:

```cpp
hideWhenViewedFromAbove = false;
```

If you want custom hidden underside edges for an imported model, load it, then edit `ship.model.lines` in the scene after loading.

## Scene Management

Scene code lives in:

```text
include/scenes/
```

The pieces are:

- `scene.h++`: base `Scene` interface.
- `main_menu.h++`: start menu with keyboard and mouse activation.
- `first_test_scene.h++`: one-planet scene with a cube portal.
- `two_planet_scene.h++`: destination scene with two planets.
- `default_scene.h++`: simple experimental scene kept for quick testing.
- `scene_manager.h++`: owns and exposes the active scene.

The base scene interface owns a world and has hooks for input, simulation, camera behavior, streaming, overlays, and transitions:

```cpp
class Scene {
public:
    virtual const char* name() const = 0;
    virtual World& world() = 0;
    virtual const World& world() const = 0;

    virtual void handleEvent(const sf::Event&, const sf::RenderWindow&) {}
    virtual bool acceptsShipInput() const { return true; }
    virtual bool showsHud() const { return true; }

    virtual void updatePhysics(float dt)
    {
        updateWorldPhysics(world(), dt);
    }

    virtual void updateCamera(Camera& camera, float dt, ShipCameraRig& rig)
    {
        updateShipCamera(camera, world().playerShip, dt, rig);
    }

    virtual void updateStreaming(const Camera& camera)
    {
        updateWorldStreaming(world(), camera);
    }

    virtual void drawOverlay(sf::RenderTarget&) {}
    virtual SceneTransition consumeTransition() { return SceneTransition::None; }
};
```

Scene transitions are small enum requests consumed by `main.cpp`. The current flow is:

```text
MainMenuScene
  -> FirstTestScene
  -> TwoPlanetScene
```

`FirstTestScene` requests `SceneTransition::TwoPlanet` when the ship passes through the cube portal volume.

### Creating Your Own Scene

Create `include/scenes/test_scene.h++`:

```cpp
#ifndef DUSK_TEST_SCENE_H
#define DUSK_TEST_SCENE_H

#include "scenes/scene.h++"

class TestScene : public Scene {
public:
    TestScene()
    {
        world_.cube.position = {2000.f, 500.f, 8000.f};

        Planet planet;
        planet.position = {-2600.f, -900.f, 6200.f};
        planet.radius = 1800.f;
        world_.planets.push_back(planet);

        ObjLoadOptions options;
        options.scale = 80.f;
        world_.playerShip.loadObjModel("assets/ships/test_ship.obj", options);
    }

    const char* name() const override
    {
        return "test";
    }

    World& world() override
    {
        return world_;
    }

    const World& world() const override
    {
        return world_;
    }

private:
    World world_;
};

#endif //DUSK_TEST_SCENE_H
```

Then include it in `main.cpp` and activate it:

```cpp
#include "scenes/test_scene.h++"

SceneManager sceneManager;
sceneManager.setScene<TestScene>();
```

### Scene-Specific Updates

Override `updatePhysics()` when a scene needs custom simulation:

```cpp
void updatePhysics(float dt) override
{
    updateWorldPhysics(world_, dt);
    world_.cube.rotationSpeed = 1.5f;
}
```

Override `handleEvent()` for menu-style input, `updateCamera()` for a scene-specific camera, `drawOverlay()` for screen-space UI, or `consumeTransition()` when a scene needs to request a switch.

## The HUD

The HUD lives in:

```text
include/rendering/hud_renderer.h++
```

It intentionally avoids font files. Instead, it draws:

- A small background rectangle.
- A thrust bar.
- Segmented digits for `0` to `100`.
- A percent sign made from primitives.
- A direction indicator color.

Current colors:

- Cyan: forward thrust.
- Red: reverse thrust.

HUD rendering happens after world rendering:

```cpp
if (sceneManager.activeScene().showsHud())
    hudRenderer.draw(window, world.playerShip);
```

Since the HUD is drawn in screen coordinates, it does not use the projector or camera.

## Adding Your Own World Object

Here is the usual pattern.

### 1. Add An Object Type

Create a header in `include/objects/asteroid.h++`:

```cpp
#ifndef DUSK_ASTEROID_H
#define DUSK_ASTEROID_H

#include "math/Vec3.h++"

/** A simple world-space asteroid. */
struct Asteroid {
    Vec3 position = {1000.f, 200.f, 5000.f};
    Vec3 velocity;
    float radius = 120.f;
};

#endif //DUSK_ASTEROID_H
```

### 2. Add It To The World

Edit `include/world/world.h++`:

```cpp
#include "objects/asteroid.h++"

struct World {
    Starfield starfield;
    Cube cube;
    bool cubeActive = true;
    std::vector<Planet> planets;
    Ship playerShip;
    Asteroid asteroid;
};
```

Now it exists in the same coordinate system as the ship.

### 3. Update It

If it has simple motion, add a system:

```cpp
inline void updateAsteroid(Asteroid& asteroid, float dt)
{
    asteroid.position += asteroid.velocity * dt;
}
```

Then call it from `updateWorldPhysics()`:

```cpp
inline void updateWorldPhysics(World& world, float dt)
{
    integrateShipPhysics(world.playerShip, dt);

    if (world.cubeActive)
        updateCube(world.cube, dt);

    updateAsteroid(world.asteroid, dt);
}
```

Or, for scene-specific behavior, keep the default `World` small and do special updates inside a custom scene's `updatePhysics()`.

### 4. Render It

For a projected point:

```cpp
const auto projected = projector.project(asteroid.position, camera, viewport);
```

For a line model, follow the pattern in `cube_renderer.h++` or `ship_renderer.h++`:

```text
world vertices
  -> view matrix
  -> clip line
  -> project endpoints
  -> draw SFML line
```

### 5. Wire It In Main

Create a renderer:

```cpp
const AsteroidRenderer asteroidRenderer(projectionConfig);
```

Draw it:

```cpp
asteroidRenderer.draw(window, world.asteroid, camera);
```

If the object only belongs to one scene, you can keep the renderer call conditional, mirror the current `cubeActive` approach, or add a scene-specific render pass later. Right now world rendering is still centralized in `main.cpp`.

## Adding Your Own Physics

Try to keep physics in `include/systems/`.

For example, if you wanted drag:

```cpp
inline void applyLinearDrag(Ship& ship, float drag, float dt)
{
    ship.velocity -= ship.velocity * drag * dt;
}
```

Then:

```cpp
inline void updateWorldPhysics(World& world, float dt)
{
    integrateShipPhysics(world.playerShip, dt);
    applyLinearDrag(world.playerShip, 0.02f, dt);

    if (world.cubeActive)
        updateCube(world.cube, dt);
}
```

If you want gravity:

```cpp
inline void applyGravity(Ship& ship, Vec3 gravity, float dt)
{
    ship.velocity += gravity * dt;
}
```

Then:

```cpp
applyGravity(world.playerShip, {0.f, -9.8f, 0.f}, dt);
```

The important idea is: physics should modify velocity and position, not rendering code.

## Adding Your Own HUD Elements

HUD code should live in `include/rendering/hud_renderer.h++` or another renderer such as:

```text
include/rendering/radar_renderer.h++
```

HUDs are screen-space. They usually should not use world projection. Draw them directly with SFML coordinates:

```cpp
sf::RectangleShape box({80.f, 12.f});
box.setPosition({20.f, 20.f});
box.setFillColor(sf::Color(255, 255, 255));
target.draw(box);
```

To add a velocity bar:

```cpp
const float speed01 = std::clamp(shipSpeed(ship) / 2000.f, 0.f, 1.f);
drawRect(target, {18.f, y - 12.f}, {72.f * speed01, 4.f}, sf::Color(255, 255, 255));
```

To add a reverse-thrust label without fonts, use primitive symbols:

- A red square for reverse.
- A cyan square for forward.
- Segmented letters if you want readable text.

If you decide to use real text later, add a font asset and make a small `FontStore` or `HudAssets` object so renderers do not load fonts every frame.

## Adding Your Own Renderer

A renderer should usually:

1. Take an `sf::RenderTarget&`.
2. Take the object to draw.
3. Take the `Camera&` if it is world-space.
4. Use `Projector` for 3D-to-2D conversion.

Example shape:

```cpp
class ThingRenderer {
public:
    explicit ThingRenderer(ProjectionConfig projectionConfig = {})
        : projector_(projectionConfig)
    {
    }

    void draw(sf::RenderTarget& target, const Thing& thing, const Camera& camera) const
    {
        const sf::Vector2u size = target.getSize();
        const Viewport viewport =
        {
            static_cast<float>(size.x),
            static_cast<float>(size.y)
        };

        const auto projected = projector_.project(thing.position, camera, viewport);

        if (!projected)
            return;

        // Draw SFML primitives here.
    }

private:
    Projector projector_;
};
```

## Common Design Rules For This Project

Keep these boundaries:

- Input maps keys to intent and settings.
- Physics updates positions and velocities.
- World owns objects.
- Scenes request transitions; `main.cpp` performs them.
- Camera decides view.
- Rendering reads state and draws.
- Projection math stays in `Projector`.

Avoid this:

```cpp
// Bad direction: renderer changes gameplay state.
ship.position.z += 10.f;
```

Prefer this:

```cpp
// Better: physics changes gameplay state, renderer only reads it.
integrateShipPhysics(ship, dt);
shipRenderer.draw(window, ship, camera);
```

## Useful Files To Start With

- `src/main.cpp`: shows the whole frame loop.
- `include/scenes/main_menu.h++`: menu input and overlay rendering.
- `include/scenes/first_test_scene.h++`: cube-portal transition example.
- `include/scenes/two_planet_scene.h++`: two-planet scene setup.
- `include/scenes/default_scene.h++`: simple experimental world and OBJ loading notes.
- `include/scenes/scene_manager.h++`: active scene ownership.
- `include/world/world.h++`: add world-coordinate objects here.
- `include/model/vector_model.h++`: shared vector-node and edge format.
- `include/io/obj_loader.h++`: OBJ-to-vector conversion.
- `include/objects/ship.h++`: example of an object with state and helper functions.
- `include/systems/ship_physics.h++`: example of a physics system.
- `include/tools/ship_controller.h++`: example of input and camera behavior.
- `include/rendering/ship_renderer.h++`: example of a line-model renderer.
- `include/rendering/planet_renderer.h++`: example of projected filled bodies.
- `include/rendering/hud_renderer.h++`: example of screen-space HUD rendering.
- `include/rendering/projector.h++`: projection, view matrix, and clipping.

## Current Limitations

This is still intentionally tiny:

- No depth buffer.
- No triangle rasterizer.
- OBJ loading only extracts vertices and wire edges.
- No general collision detection; the cube portal uses a small scene-specific trigger.
- No time-step accumulator.
- No real asset system.
- HUD uses primitive shapes instead of text/fonts; the menu uses the bundled Jersey 15 font.

Those are good next steps, but the current shape is enough to experiment with fake-3D projection, starfields, scene flow, planet rendering, line models, and simple space-flight physics.
