//
// Created by Mykyta Khomiakov on 24/07/2026.
//

#ifndef DUSK_COLLISION_BODY_H
#define DUSK_COLLISION_BODY_H

#include "math/Vec3.h++"
#include <algorithm>
#include <cstddef>
#include <cmath>
#include <vector>

/** Object categories used in collision hit records. */
enum class CollisionObjectType {
    Ship,
    Cube,
    Planet
};

/** A single collision registered against an object during the current frame. */
struct CollisionHit {
    /** Type of the object this body touched. */
    CollisionObjectType otherType = CollisionObjectType::Ship;

    /** Index for collection-backed objects, or zero for singular objects. */
    std::size_t otherIndex = 0;

    /** Unit direction pointing from this object toward the other object. */
    Vec3 normal;

    /** Amount the two collision volumes overlap by. */
    float penetration = 0.f;
};

/** Per-object collision state. Detection runs automatically from World. */
struct CollisionBody {
    /** Set to false for scenery or helpers that should not produce collision hits. */
    bool detectsCollisions = true;

    /** Hits registered during the latest world physics update. */
    std::vector<CollisionHit> hits;

    bool isColliding() const
    {
        return !hits.empty();
    }

    bool collidingWith(CollisionObjectType type) const
    {
        return std::any_of(
            hits.begin(),
            hits.end(),
            [type](const CollisionHit& hit) {
                return hit.otherType == type;
            }
        );
    }

    bool collidingWith(CollisionObjectType type, std::size_t index) const
    {
        return std::any_of(
            hits.begin(),
            hits.end(),
            [type, index](const CollisionHit& hit) {
                return hit.otherType == type && hit.otherIndex == index;
            }
        );
    }

    void clear()
    {
        hits.clear();
    }
};

/** Registers a hit on one side of a collision pair. */
inline void addCollisionHit(
    CollisionBody& body,
    CollisionObjectType otherType,
    std::size_t otherIndex,
    const Vec3& normal,
    float penetration
)
{
    body.hits.push_back({otherType, otherIndex, normal, penetration});
}

/** Detects overlap between two spherical collision volumes. */
inline bool detectSphereCollision(
    const Vec3& firstPosition,
    float firstRadius,
    const Vec3& secondPosition,
    float secondRadius,
    Vec3& normal,
    float& penetration
)
{
    const float collisionDistance = std::max(0.f, firstRadius) + std::max(0.f, secondRadius);
    const Vec3 offset = secondPosition - firstPosition;
    const float distanceSquared = dot(offset, offset);

    if (distanceSquared > collisionDistance * collisionDistance)
        return false;

    const float distance = std::sqrt(distanceSquared);
    normal = distance > 0.f ? offset / distance : Vec3{1.f, 0.f, 0.f};
    penetration = collisionDistance - distance;
    return true;
}

/** Detects a moving sphere crossing or overlapping a static sphere during this frame. */
inline bool detectSweptSphereCollision(
    const Vec3& movingStart,
    const Vec3& movingEnd,
    float movingRadius,
    const Vec3& staticPosition,
    float staticRadius,
    Vec3& normal,
    float& penetration
)
{
    const Vec3 movement = movingEnd - movingStart;
    const float movementLengthSquared = dot(movement, movement);
    Vec3 closestPoint = movingEnd;

    if (movementLengthSquared > 0.f)
    {
        const float t = std::clamp(
            dot(staticPosition - movingStart, movement) / movementLengthSquared,
            0.f,
            1.f
        );
        closestPoint = movingStart + movement * t;
    }

    return detectSphereCollision(
        closestPoint,
        movingRadius,
        staticPosition,
        staticRadius,
        normal,
        penetration
    );
}

#endif //DUSK_COLLISION_BODY_H
