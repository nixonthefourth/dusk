//
// Created by Mykyta Khomiakov on 22/07/2026.
//

#ifndef DUSK_STARFIELD_H
#define DUSK_STARFIELD_H

#include "objects/star.h++"
#include <random>
#include <vector>

struct StarfieldConfig {
    int starCount = 1000;
    float spaceSize = 10000.f;
    float minDepth = 100.f;
    float maxDepth = 20000.f;
};

inline std::vector<Star> createStarfield(const StarfieldConfig& config = {})
{
    std::vector<Star> stars;
    stars.reserve(config.starCount);

    std::mt19937 gen(std::random_device{}());
    std::uniform_real_distribution<float> xDist(-config.spaceSize, config.spaceSize);
    std::uniform_real_distribution<float> yDist(-config.spaceSize, config.spaceSize);
    std::uniform_real_distribution<float> zDist(config.minDepth, config.maxDepth);

    for (int i = 0; i < config.starCount; ++i)
    {
        stars.push_back(
        {
            {
                xDist(gen),
                yDist(gen),
                zDist(gen)
            }
        });
    }

    return stars;
}

#endif //DUSK_STARFIELD_H
