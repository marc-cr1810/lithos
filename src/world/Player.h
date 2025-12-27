#ifndef PLAYER_H
#define PLAYER_H

#include <GL/glew.h>
#include <glm/glm.hpp>

class World;

enum Player_Movement { P_FORWARD, P_BACKWARD, P_LEFT, P_RIGHT, P_UP, P_DOWN };

class Player {
public:
  // Attributes
  glm::vec3 Position;
  glm::vec3 Velocity;
  glm::vec3 Front;
  glm::vec3 Right;
  glm::vec3 Up;
  glm::vec3 WorldUp;

  // Euler Angles
  float Yaw;
  float Pitch;

  // Options
  float MovementSpeed;
  float MouseSensitivity;

  // Physics
  float Gravity;
  float JumpForce;
  bool IsGrounded;
  bool FlyMode = false;
  bool IsSprinting = false;
  float SprintSpeed;

  // Checks if the player at the given position collides with any blocks
  bool CheckCollision(glm::vec3 pos, const World &world);

  Player(glm::vec3 position = glm::vec3(0.0f, 0.0f, 0.0f));

  void ProcessKeyboard(bool forward, bool backward, bool left, bool right,
                       bool up, bool down, float deltaTime, const World &world);
  void ProcessMouseMovement(float xoffset, float yoffset,
                            bool constrainPitch = true);
  void ProcessJump(bool jump, const World &world);

  void Update(float deltaTime, const World &world);

  glm::vec3 GetEyePosition() const;

private:
  void updateCameraVectors();
};

#endif
