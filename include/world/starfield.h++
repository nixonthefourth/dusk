//
// Created by Mykyta Khomiakov on 22/07/2026.
//

#ifndef DUSK_STARFIELD_H
#define DUSK_STARFIELD_H

#include "objects/star.h++"
#include "math/Vec3.h++"
#include <cmath>
#include <random>
#include <vector>

/** Settings for the camera-centered, recycled star volume. */
struct StarfieldConfig {
    /** Number of stars kept alive and reused forever. */
    int starCount = 3000;

    /** Half-size of the cubic star volume around its current center. */
    float radius = 30000.f;

    /** Recenter the field when the camera travels this fraction of the radius. */
    float recycleThreshold = 0.5f;
};

/** Maintains an endless-looking starfield without allocating millions of stars. */
class Starfield {
public:
    /** Creates the initial pool of stars around the origin. */
    explicit Starfield(const StarfieldConfig& config = {})
        : config_(config)
    {
        stars_.reserve(config_.starCount);

        for (int i = 0; i < config_.starCount; ++i)
            stars_.push_back({randomPositionAround(center_)});
    }

    /** Recenters and recycles the star pool around the camera when needed. */
    void update(const Vec3& cameraPosition)
    {
        const Vec3 cameraOffset = cameraPosition - center_;
        const float triggerDistance = config_.radius * config_.recycleThreshold;

        if (std::abs(cameraOffset.x) > triggerDistance ||
            std::abs(cameraOffset.y) > triggerDistance ||
            std::abs(cameraOffset.z) > triggerDistance)
        {
            center_ = cameraPosition;

            for (Star& star : stars_)
                star.position = randomPositionAround(center_);

            return;
        }

        for (Star& star : stars_)
        {
            if (isOutsideField(star.position, center_))
                star.position = randomPositionAround(center_);
        }
    }

    /** Returns the live stars to render this frame. */
    const std::vector<Star>& stars() const
    {
        return stars_;
    }

    /** Returns the current field radius used for draw-distance tuning. */
    float radius() const
    {
        return config_.radius;
    }

private:
    StarfieldConfig config_;
    Vec3 center_;
    std::vector<Star> stars_;
    std::mt19937 gen_ = std::mt19937(std::random_device{}());

    /** Returns a random coordinate offset inside the current field radius. */
    float randomOffset()
    {
        std::uniform_real_distribution<float> dist(-config_.radius, config_.radius);
        return dist(gen_);
    }

    /** Picks a random world-space position inside the field centered at center. */
    Vec3 randomPositionAround(const Vec3& center)
    {
        return
        {
            center.x + randomOffset(),
            center.y + randomOffset(),
            center.z + randomOffset()
        };
    }

    /** Checks whether a star has drifted outside the current recycled volume. */
    bool isOutsideField(const Vec3& position, const Vec3& center) const
    {
        return
            std::abs(position.x - center.x) > config_.radius ||
            std::abs(position.y - center.y) > config_.radius ||
            std::abs(position.z - center.z) > config_.radius;
    }

};

#endif //DUSK_STARFIELD_H
