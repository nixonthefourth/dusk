//
// Created by Mykyta Khomiakov on 22/07/2026.
//

#ifndef DUSK_PROJECTOR_H
#define DUSK_PROJECTOR_H

#include "math/Mat4.h++"
#include "math/Vec2.h++"
#include "math/Vec3.h++"
#include "tools/camera.h++"
#include <algorithm>
#include <cmath>
#include <optional>

/** Dimensions of the render target in pixels. */
struct Viewport {
    float width = 0.f;
    float height = 0.f;
};

/** A point after perspective projection into screen space. */
struct ProjectedPoint {
    /** Pixel position on the 2D render target. */
    Vec2 position;

    /** Positive camera-space depth used for fading and sorting. */
    float depth = 0.f;
};

/** Near/far clipping planes for perspective projection. */
struct ProjectionConfig {
    float nearPlane = 1.f;
    float farPlane = 20000.f;
};

/** A line segment expressed in camera space after frustum clipping. */
struct CameraSpaceLine {
    Vec3 start;
    Vec3 end;
};

/** Camera-space viewing frustum used for point and line clipping. */
struct Frustum {
    float nearPlane = 1.f;
    float farPlane = 20000.f;
    float halfVerticalFovTan = 1.f;
    float aspect = 1.f;

    /** Returns true when a camera-space point is inside the frustum. */
    bool contains(const Vec3& point, float radius = 0.f) const
    {
        if (point.z < nearPlane - radius || point.z > farPlane + radius)
            return false;

        const float yLimit = point.z * halfVerticalFovTan + radius;
        const float xLimit = yLimit * aspect + radius;

        return
            point.x >= -xLimit &&
            point.x <= xLimit &&
            point.y >= -yLimit &&
            point.y <= yLimit;
    }
};

/** Converts 3D world/camera coordinates into fake-3D screen coordinates. */
class Projector {
public:
    /** Creates a projector with the supplied clipping planes. */
    explicit Projector(ProjectionConfig config = {})
        : config_(config)
    {
    }

    /** Projects a world-space point by first transforming it through the view matrix. */
    std::optional<ProjectedPoint> project(
        const Vec3& worldPosition,
        const Camera& camera,
        const Viewport& viewport
    ) const
    {
        return projectCameraSpace(transformPoint(createViewMatrix(camera), worldPosition), camera, viewport);
    }

    /** Projects a point that is already in camera space, returning null when clipped. */
    std::optional<ProjectedPoint> projectCameraSpace(
        const Vec3& cameraSpace,
        const Camera& camera,
        const Viewport& viewport
    ) const
    {
        const Frustum frustum = createFrustum(camera, viewport);

        if (!frustum.contains(cameraSpace))
            return std::nullopt;

        const float focalLength = focalLengthFor(camera, viewport);

        return ProjectedPoint
        {
            {
                viewport.width * 0.5f + (cameraSpace.x * focalLength / cameraSpace.z),
                viewport.height * 0.5f - (cameraSpace.y * focalLength / cameraSpace.z)
            },
            cameraSpace.z
        };
    }

    /** Clips a camera-space line segment against the near/far and side frustum planes. */
    std::optional<CameraSpaceLine> clipLineCameraSpace(
        Vec3 start,
        Vec3 end,
        const Camera& camera,
        const Viewport& viewport
    ) const
    {
        const Frustum frustum = createFrustum(camera, viewport);
        float enter = 0.f;
        float exit = 1.f;
        const Vec3 delta = end - start;

        const auto clip = [&](float numerator, float denominator) {
            if (denominator == 0.f)
                return numerator <= 0.f;

            const float t = numerator / denominator;

            if (denominator > 0.f)
            {
                if (t > exit)
                    return false;

                enter = std::max(enter, t);
            }
            else
            {
                if (t < enter)
                    return false;

                exit = std::min(exit, t);
            }

            return true;
        };

        const float ySlope = frustum.halfVerticalFovTan;
        const float xSlope = frustum.halfVerticalFovTan * frustum.aspect;

        if (!clip(frustum.nearPlane - start.z, delta.z))
            return std::nullopt;

        if (!clip(start.z - frustum.farPlane, -delta.z))
            return std::nullopt;

        if (!clip(start.x - xSlope * start.z, xSlope * delta.z - delta.x))
            return std::nullopt;

        if (!clip(-start.x - xSlope * start.z, xSlope * delta.z + delta.x))
            return std::nullopt;

        if (!clip(start.y - ySlope * start.z, ySlope * delta.z - delta.y))
            return std::nullopt;

        if (!clip(-start.y - ySlope * start.z, ySlope * delta.z + delta.y))
            return std::nullopt;

        return CameraSpaceLine
        {
            start + delta * enter,
            start + delta * exit
        };
    }

    /** Builds the world-to-camera transform from camera position and yaw/pitch/roll. */
    Mat4 createViewMatrix(const Camera& camera) const
    {
        const float sinYaw = std::sin(camera.yaw);
        const float cosYaw = std::cos(camera.yaw);
        const float sinPitch = std::sin(camera.pitch);
        const float cosPitch = std::cos(camera.pitch);

        const Vec3 forward = normalized(
        {
            sinYaw * cosPitch,
            sinPitch,
            cosYaw * cosPitch
        });
        const Vec3 worldUp = {0.f, 1.f, 0.f};
        Vec3 right = normalized(cross(worldUp, forward));

        if (length(right) == 0.f)
            right = {1.f, 0.f, 0.f};

        Vec3 up = normalized(cross(forward, right));

        if (camera.roll != 0.f)
        {
            const float sinRoll = std::sin(camera.roll);
            const float cosRoll = std::cos(camera.roll);
            const Vec3 rolledRight = right * cosRoll + up * sinRoll;
            const Vec3 rolledUp = up * cosRoll - right * sinRoll;

            right = rolledRight;
            up = rolledUp;
        }

        Mat4 matrix;

        matrix.m[0][0] = right.x;
        matrix.m[0][1] = right.y;
        matrix.m[0][2] = right.z;

        matrix.m[1][0] = up.x;
        matrix.m[1][1] = up.y;
        matrix.m[1][2] = up.z;

        matrix.m[2][0] = forward.x;
        matrix.m[2][1] = forward.y;
        matrix.m[2][2] = forward.z;

        matrix.m[0][3] = -(
            matrix.m[0][0] * camera.position.x +
            matrix.m[0][1] * camera.position.y +
            matrix.m[0][2] * camera.position.z
        );

        matrix.m[1][3] = -(
            matrix.m[1][0] * camera.position.x +
            matrix.m[1][1] * camera.position.y +
            matrix.m[1][2] * camera.position.z
        );

        matrix.m[2][3] = -(
            matrix.m[2][0] * camera.position.x +
            matrix.m[2][1] * camera.position.y +
            matrix.m[2][2] * camera.position.z
        );

        return matrix;
    }

private:
    ProjectionConfig config_;

    static constexpr float pi = 3.14159265358979323846f;

    static float degreesToRadians(float degrees)
    {
        return degrees * pi / 180.f;
    }

    /** Returns the focal length in pixels for the current field-of-view and viewport. */
    float focalLengthFor(const Camera& camera, const Viewport& viewport) const
    {
        return (viewport.height * 0.5f) / std::tan(degreesToRadians(camera.fov) * 0.5f);
    }

    /** Builds a camera-space frustum from projection settings and viewport aspect. */
    Frustum createFrustum(const Camera& camera, const Viewport& viewport) const
    {
        return
        {
            config_.nearPlane,
            config_.farPlane,
            std::tan(degreesToRadians(camera.fov) * 0.5f),
            viewport.width / viewport.height
        };
    }
};

#endif //DUSK_PROJECTOR_H
