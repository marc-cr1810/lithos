#ifndef COMPONENTS_H
#define COMPONENTS_H

#include "../world/Block.h"
#include <glm/glm.hpp>

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

struct CameraComponent {
  glm::vec3 front;
  glm::vec3 right;
  glm::vec3 up;
  glm::vec3 worldUp;
  float yaw;
  float pitch;
  float zoom;
};

struct InputComponent {
  float mouseSensitivity;
  float movementSpeed;
  float sprintSpeed;
  bool isSprinting;
  bool flyMode;
  bool isGrounded;
};

struct PlayerTag {};

#endif
