#ifndef SYSTEMS_H
#define SYSTEMS_H

#include <entt/entt.hpp>
#include <glm/glm.hpp>
#include "../render/Shader.h"

class World; // Forward declaration

class PhysicsSystem {
public:
    static void Update(entt::registry& registry, float dt);
};

class CollisionSystem {
public:
    static void Update(entt::registry& registry, World& world, float dt);
};

class RenderSystem {
public:
    static void Render(entt::registry& registry, World& world, Shader& shader, const glm::mat4& viewProjection);
private:
    static void initCubeMesh();
    static unsigned int cubeVAO, cubeVBO;
};

#endif
