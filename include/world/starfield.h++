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
    int starCount = 3000;
    float radius = 90000.f; // was 30000.f
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

    /**
     * Keeps the star volume centred on the camera by wrapping: a star that falls more than one
     * field radius behind on any axis reappears the same distance ahead on that axis. Nothing is
     * ever re-scattered in view, so the field stays seamless even at cruise speed, and stars keep
     * streaming past to show how fast the ship is going.
     */
    void update(const Vec3& cameraPosition)
    {
        center_ = cameraPosition;
        const float span = config_.radius * 2.f;

        const auto wrapAxis = [&](float& value, float centre)
        {
            const float offset = value - centre;

            if (offset > config_.radius || offset < -config_.radius)
                value -= span * std::floor((offset + config_.radius) / span);
        };

        for (Star& star : stars_)
        {
            wrapAxis(star.position.x, center_.x);
            wrapAxis(star.position.y, center_.y);
            wrapAxis(star.position.z, center_.z);
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

};

#endif //DUSK_STARFIELD_H
