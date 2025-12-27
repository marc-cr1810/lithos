#ifndef SYSTEMS_H
#define SYSTEMS_H

#include "../render/Shader.h"
#include <entt/entt.hpp>
#include <glm/glm.hpp>

class World; // Forward declaration
class Camera;

class PhysicsSystem {
public:
  static void Update(entt::registry &registry, float dt);
};

class CollisionSystem {
public:
  static void Update(entt::registry &registry, World &world, float dt);
};

class RenderSystem {
public:
  static void Render(entt::registry &registry, World &world, Shader &shader,
                     const glm::mat4 &viewProjection);

private:
  static void initCubeMesh();
  static unsigned int cubeVAO, cubeVBO;
};

class Camera; // Forward declaration

class PlayerControlSystem {
public:
  static void Update(entt::registry &registry, bool forward, bool backward,
                     bool left, bool right, bool up, bool down, float dt,
                     const World &world);
};

class CameraSystem {
public:
  static void Update(entt::registry &registry, Camera &camera);
};

#endif
