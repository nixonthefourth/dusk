//
// Created by Mykyta Khomiakov on 22/07/2026.
//

#ifndef DUSK_PROJECTOR_H
#define DUSK_PROJECTOR_H

#include "math/Vec2.h++"
#include "math/Vec3.h++"
#include "tools/camera.h++"
#include <cmath>
#include <optional>

struct Viewport {
    float width = 0.f;
    float height = 0.f;
};

struct ProjectedPoint {
    Vec2 position;
    float depth = 0.f;
};

class Projector {
public:
    explicit Projector(float nearPlane = 1.f)
        : nearPlane_(nearPlane)
    {
    }

    std::optional<ProjectedPoint> project(
        const Vec3& worldPosition,
        const Camera& camera,
        const Viewport& viewport
    ) const
    {
        const Vec3 cameraSpace = worldToCameraSpace(worldPosition, camera);

        if (cameraSpace.z <= nearPlane_)
            return std::nullopt;

        const float focalLength = (viewport.height * 0.5f) / std::tan(degreesToRadians(camera.fov) * 0.5f);

        return ProjectedPoint
        {
            {
                viewport.width * 0.5f + (cameraSpace.x * focalLength / cameraSpace.z),
                viewport.height * 0.5f - (cameraSpace.y * focalLength / cameraSpace.z)
            },
            cameraSpace.z
        };
    }

private:
    float nearPlane_ = 1.f;

    static constexpr float pi = 3.14159265358979323846f;

    static float degreesToRadians(float degrees)
    {
        return degrees * pi / 180.f;
    }

    static Vec3 worldToCameraSpace(const Vec3& worldPosition, const Camera& camera)
    {
        const Vec3 relative = worldPosition - camera.position;

        const float sinYaw = std::sin(camera.yaw);
        const float cosYaw = std::cos(camera.yaw);
        const Vec3 yawed =
        {
            cosYaw * relative.x - sinYaw * relative.z,
            relative.y,
            sinYaw * relative.x + cosYaw * relative.z
        };

        const float sinPitch = std::sin(camera.pitch);
        const float cosPitch = std::cos(camera.pitch);
        const Vec3 pitched =
        {
            yawed.x,
            cosPitch * yawed.y - sinPitch * yawed.z,
            sinPitch * yawed.y + cosPitch * yawed.z
        };

        const float sinRoll = std::sin(camera.roll);
        const float cosRoll = std::cos(camera.roll);

        return
        {
            cosRoll * pitched.x + sinRoll * pitched.y,
            -sinRoll * pitched.x + cosRoll * pitched.y,
            pitched.z
        };
    }
};

#endif //DUSK_PROJECTOR_H
