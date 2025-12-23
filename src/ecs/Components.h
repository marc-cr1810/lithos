#ifndef COMPONENTS_H
#define COMPONENTS_H

#include <glm/glm.hpp>
#include "../world/Block.h"

struct TransformComponent {
    glm::vec3 position;
    glm::vec3 rotation;
    glm::vec3 scale;
};

struct VelocityComponent {
    glm::vec3 velocity;
};

struct GravityComponent {
    float strength; // e.g., 9.8f
};

struct ColliderComponent {
    glm::vec3 size; // AABB half-extents or full size? Let's use full size.
    // 0.98f, 0.98f, 0.98f for blocks to avoid z-fighting
};

struct BlockComponent {
    BlockType type; // The block this entity represents
};

#endif
