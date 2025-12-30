#include "Player.h"
#include "World.h"
#include <algorithm>
#include <cmath>
#include <iostream>

Player::Player(glm::vec3 position)
    : Front(glm::vec3(0.0f, 0.0f, -1.0f)), MovementSpeed(6.0f),
      SprintSpeed(10.5f), MouseSensitivity(0.1f), Yaw(-90.0f), Pitch(0.0f),
      WorldUp(glm::vec3(0.0f, 1.0f, 0.0f)), Velocity(0.0f), Gravity(45.0f),
      JumpForce(13.0f), IsGrounded(false) {
  Position = position;
  updateCameraVectors();
}

// Calculates the front vector from the Camera's (Euler) Angles
void Player::updateCameraVectors() {
  glm::vec3 front;
  front.x = cos(glm::radians(Yaw)) * cos(glm::radians(Pitch));
  front.y = sin(glm::radians(Pitch));
  front.z = sin(glm::radians(Yaw)) * cos(glm::radians(Pitch));
  Front = glm::normalize(front);
  Right = glm::normalize(glm::cross(Front, WorldUp));
  Up = glm::normalize(glm::cross(Right, Front));
}

// Process keyboard input
void Player::ProcessKeyboard(bool forward, bool backward, bool left, bool right,
                             bool up, bool down, float deltaTime,
                             const World &world) {
  float speed = IsSprinting ? SprintSpeed : MovementSpeed;
  float velocity = speed * deltaTime;

  // Calculate movement direction
  glm::vec3 flatFront = glm::normalize(glm::vec3(Front.x, 0.0f, Front.z));
  glm::vec3 flatRight = glm::normalize(glm::vec3(Right.x, 0.0f, Right.z));

  glm::vec3 moveDir(0.0f);
  if (forward)
    moveDir += flatFront;
  if (backward)
    moveDir -= flatFront;
  if (left)
    moveDir -= flatRight;
  if (right)
    moveDir += flatRight;

  if (FlyMode)
    velocity *= 4.0f; // Fly faster

  // Normalize input vector
  if (glm::length(moveDir) > 0.0f) {
    moveDir = glm::normalize(moveDir);
  }

  if (glm::length(moveDir) > 0.0f || (FlyMode && (up || down))) {
    // Apply speed
    glm::vec3 finalMove = moveDir * velocity;

    if (FlyMode) {
      // Free Fly (Noclip)
      glm::vec3 flyDir(0.0f);
      if (forward)
        flyDir += Front;
      if (backward)
        flyDir -= Front;
      if (left)
        flyDir -= Right;
      if (right)
        flyDir += Right;
      if (up)
        flyDir += WorldUp;
      if (down)
        flyDir -= WorldUp;

      if (glm::length(flyDir) > 0.0f) {
        Position += glm::normalize(flyDir) * velocity;
      }

    } else {
      // Try X
      glm::vec3 tryX = Position;
      tryX.x += finalMove.x;
      if (!CheckCollision(tryX, world))
        Position.x = tryX.x;

      // Try Z
      glm::vec3 tryZ = Position;
      tryZ.z += finalMove.z;
      // Use current X
      tryZ.x = Position.x;
      if (!CheckCollision(tryZ, world))
        Position.z = tryZ.z;
    }
  }
}

void Player::ProcessMouseMovement(float xoffset, float yoffset,
                                  bool constrainPitch) {
  xoffset *= MouseSensitivity;
  yoffset *= MouseSensitivity;

  Yaw += xoffset;
  Pitch += yoffset;

  if (constrainPitch) {
    if (Pitch > 89.0f)
      Pitch = 89.0f;
    if (Pitch < -89.0f)
      Pitch = -89.0f;
  }

  // Update Front, Right and Up Vectors using the updated Euler angles
  updateCameraVectors();
}

void Player::ProcessJump(bool jump, const World &world) {
  // Check if in water
  ChunkBlock b = world.getBlock((int)floor(Position.x), (int)floor(Position.y),
                                (int)floor(Position.z));
  bool inWater = (b.getType() == WATER || b.getType() == LAVA);

  if (jump) {
    if (IsGrounded) {
      Velocity.y = JumpForce;
      IsGrounded = false;
    } else if (inWater) {
      // Check if head is above water (Surface Jump)
      ChunkBlock headBlock =
          world.getBlock((int)floor(Position.x), (int)floor(Position.y + 1.5f),
                         (int)floor(Position.z));
      bool headInWater =
          (headBlock.getType() == WATER || headBlock.getType() == LAVA);

      if (!headInWater) {
        // Head is out, feet are in. Apply strong impulse to exit water.
        Velocity.y = 8.0f; // Jump out! (Stronger impulse)
      } else {
        // Fully underwater - Swim up
        // Velocity.y += 0.5f;
        if (Velocity.y < 2.0f)
          Velocity.y += 0.8f;
      }
    }
  }
}

void Player::Update(float deltaTime, const World &world) {
  if (FlyMode) {
    // No gravity, no collision.
    // Maybe some drag?
    Velocity = glm::vec3(0.0f);
    return;
  }

  // Water Physics
  bool inWater = false;
  ChunkBlock b = world.getBlock((int)floor(Position.x), (int)floor(Position.y),
                                (int)floor(Position.z));
  if (b.getType() == WATER || b.getType() == LAVA) {
    inWater = true;
  } else {
    // Check eye level too?
    ChunkBlock b2 =
        world.getBlock((int)floor(Position.x), (int)floor(Position.y + 1.5f),
                       (int)floor(Position.z));
    if (b2.getType() == WATER || b2.getType() == LAVA)
      inWater = true;
  }

  // Apply Gravity
  float effGravity = Gravity;
  if (inWater)
    effGravity *= 0.2f; // Buoyancy

  Velocity.y -= effGravity * deltaTime;

  // Air Resistance (Drag)
  float drag = 2.0f;
  if (inWater)
    drag = 3.0f; // Lower drag in water to prevent "stuck in sludge" feel

  Velocity.x -= Velocity.x * drag * deltaTime;
  Velocity.z -= Velocity.z * drag * deltaTime;
  Velocity.y -= Velocity.y * drag * deltaTime;

  // Terminal Velocity (Safety Clamp)
  if (Velocity.y < -78.4f)
    Velocity.y = -78.4f;
  if (inWater && Velocity.y < -5.0f)
    Velocity.y = -5.0f; // Slower terminal velocity in water

  // Check Ceiling Collision
  if (Velocity.y > 0.0f) {
    glm::vec3 tryPos = Position;
    tryPos.y += Velocity.y * deltaTime;
    if (CheckCollision(tryPos, world)) {
      Velocity.y = 0.0f;
      // Don't move up
    }
  }

  // Apply Movement
  Position.y += Velocity.y * deltaTime;

  // Floor Collision
  float playerHeight = 1.8f;
  float eyeHeight = 1.6f;
  float feetY = Position.y - eyeHeight;

  // Use a slightly larger epsilon for vertical check to stabilize "standing"
  // detection and ensure we check the block *below* the feet when exactly at
  // integer coordinates.
  int blockY = (int)floor(feetY - 0.1f);

  float playerWidth = 0.6f;
  // Shrink the floor check bounding box slightly so we don't treat adjacent
  // walls as "ground" This prevents the player from snapping up when walking
  // into walls or "wall jumping". Increased to 0.1 to handle chunk boundary
  // precision issues.
  float boundsEpsilon = 0.1f;

  int minBlockX = (int)floor(Position.x - playerWidth / 2.0f + boundsEpsilon);
  int maxBlockX = (int)floor(Position.x + playerWidth / 2.0f - boundsEpsilon);
  int minBlockZ = (int)floor(Position.z - playerWidth / 2.0f + boundsEpsilon);
  int maxBlockZ = (int)floor(Position.z + playerWidth / 2.0f - boundsEpsilon);

  bool hitGround = false;

  // Only check floor collision if we are falling or standing still (or moving
  // slightly down due to gravity)
  if (Velocity.y <= 0.0f && blockY >= -128 && blockY < 512) {
    for (int x = minBlockX; x <= maxBlockX; ++x) {
      for (int z = minBlockZ; z <= maxBlockZ; ++z) {
        if (world.getBlock(x, blockY, z).isSolid()) {
          hitGround = true;
          // Optimization: Break early if ground is found
          // (Wait, we can't just set x = maxBlockX + 1 because the outer loop
          // continues? Actually, breaking inner loop is fine, we just need one
          // solid block to be grounded)
          goto found_ground;
        }
      }
    }
  }
found_ground:

  if (hitGround) {
    float newY = (float)(blockY + 1) + eyeHeight;
    if (std::abs(newY - Position.y) > 0.1f) {
      printf("[PHYSICS] SNAP DETECTED! PosY: %.4f -> %.4f. BlockY: %d. FeetY: "
             "%.4f\n",
             Position.y, newY, blockY, feetY);
    }
    Position.y = newY;
    Velocity.y = 0.0f;
    IsGrounded = true;
  } else {
    IsGrounded = false;
  }
}

bool Player::CheckCollision(glm::vec3 pos, const World &world) {
  float playerWidth = 0.6f;
  float playerHeight = 1.8f;
  float eyeHeight = 1.6f;

  // Shrink the bounding box slightly to prevent treating "touching" as
  // overlapping
  float epsilon = 0.05f; // Small buffer
  float minX = pos.x - playerWidth / 2.0f;
  float maxX = pos.x + playerWidth / 2.0f;
  float minZ = pos.z - playerWidth / 2.0f;
  float maxZ = pos.z + playerWidth / 2.0f;
  // Contract Y to allow sliding on floor/ceiling without getting stuck
  float minY = pos.y - eyeHeight + epsilon;
  float maxY = pos.y - eyeHeight + playerHeight - epsilon;

  int minBlockX = (int)floor(minX);
  int maxBlockX = (int)floor(maxX);
  int minBlockY = (int)floor(minY);
  int maxBlockY = (int)floor(maxY);
  int minBlockZ = (int)floor(minZ);
  int maxBlockZ = (int)floor(maxZ);

  for (int x = minBlockX; x <= maxBlockX; ++x) {
    for (int y = minBlockY; y <= maxBlockY; ++y) {
      for (int z = minBlockZ; z <= maxBlockZ; ++z) {
        if (world.getBlock(x, y, z).isSolid())
          return true;
      }
    }
  }
  return false;
}

glm::vec3 Player::GetEyePosition() const { return Position; }
