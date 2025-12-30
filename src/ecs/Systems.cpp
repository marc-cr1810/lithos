#include "Systems.h"
#include "../debug/Logger.h"
#include "../render/Camera.h"
#include "../world/World.h"
#include "Components.h"
#include <cmath>
#include <glm/gtc/matrix_transform.hpp>
#include <iostream>

// --- Physics System ---

void PhysicsSystem::Update(entt::registry &registry, float dt) {
  auto view =
      registry.view<TransformComponent, VelocityComponent, GravityComponent>();

  view.each([dt](auto entity, auto &transform, auto &vel, auto &gravity) {
    // Apply Gravity
    vel.velocity.y -= gravity.strength * dt;

    // Apply Velocity (Collision system will correct this later ideally, but for
    // now apply first)
    transform.position += vel.velocity * dt;
  });
}

// --- Collision System ---

void CollisionSystem::Update(entt::registry &registry, World &world, float dt) {
  auto view = registry.view<TransformComponent, VelocityComponent,
                            ColliderComponent, BlockComponent>();

  view.each([&world, dt, &registry](auto entity, auto &transform, auto &vel,
                                    auto &collider, auto &block) {
    // Simple AABB vs Voxel collision
    // Check center point + offsets?

    // Check if bottom center is inside a solid block
    // Transform position is center of entity
    glm::vec3 bottomPoint =
        transform.position - glm::vec3(0, collider.size.y * 0.5f, 0);
    glm::vec3 checkPos =
        bottomPoint + glm::vec3(0, -0.05f, 0); // Check slightly below

    int bx = std::floor(checkPos.x);
    int by = std::floor(checkPos.y);
    int bz = std::floor(checkPos.z);

    ChunkBlock b = world.getBlock(bx, by, bz);
    if (b.getType() != BlockType::AIR && b.getType() != BlockType::WATER &&
        b.getType() != BlockType::LAVA) {
      // Collision with ground
      if (vel.velocity.y < 0) {
        // Stop
        vel.velocity = glm::vec3(0);

        // Re-solidify
        // Snap to nearest block integer
        int ix = std::floor(transform.position.x);
        int iy = std::floor(
            transform.position.y); // Use center Y, but might need to offset?
        // Ideally, align with grid.
        // If y is 65.2, block was Falling from 66.
        // Target is 65.

        // Let's use the block coordinate directly above the collision
        world.setBlock(bx, by + 1, bz, block.type);

        // Destroy Entity
        registry.destroy(entity);
      }
    }
  });
}

// --- Render System ---
unsigned int RenderSystem::cubeVAO = 0;
unsigned int RenderSystem::cubeVBO = 0;

// --- Player Control System ---

void PlayerControlSystem::Update(entt::registry &registry, bool forward,
                                 bool backward, bool left, bool right, bool up,
                                 bool down, float dt, const World &world) {
  auto view =
      registry.view<TransformComponent, VelocityComponent, GravityComponent,
                    CameraComponent, InputComponent>();

  view.each([forward, backward, left, right, up, down, dt,
             &world](auto entity, auto &transform, auto &vel, auto &gravity,
                     auto &cam, auto &input) {
    // Helper for collision
    auto checkCollision = [&world](glm::vec3 pos) -> bool {
      float playerWidth = 0.6f;
      float playerHeight = 1.8f;
      float eyeHeight = 1.6f;
      // Increased epsilon to prevent interacting with walls
      float epsilon = 0.1f;
      float minX = pos.x - playerWidth / 2.0f + epsilon;
      float maxX = pos.x + playerWidth / 2.0f - epsilon;
      float minZ = pos.z - playerWidth / 2.0f + epsilon;
      float maxZ = pos.z + playerWidth / 2.0f - epsilon;

      // Vertical margin
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
    };

    // --- PHASE 1: Resolve Y Collision (from PhysicsSystem gravity) ---
    if (checkCollision(transform.position)) {
      if (vel.velocity.y < 0) {
        // Falling down into floor - Snap up
        float eyeHeight = 1.6f;
        float feetY = transform.position.y - eyeHeight;

        // Use offset to stabilize integer boundary
        int blockY = (int)floor(feetY - 0.1f);

        // Snap to top of blockY:
        transform.position.y = (float)(blockY + 1) + eyeHeight;

        vel.velocity.y = 0.0f;
        input.isGrounded = true;
      } else if (vel.velocity.y > 0) {
        // Jumping up into ceiling - Snap down
        while (checkCollision(transform.position)) {
          transform.position.y -= 0.01f;
        }
        vel.velocity.y = 0.0f;
      }
    } else {
      // Not inside block, but check if we are grounded (standing on block)
      if (!input.flyMode) {
        glm::vec3 checkBelow = transform.position;
        // Check slightly deeper to catch ground
        checkBelow.y -= 0.1f;
        if (checkCollision(checkBelow)) {
          input.isGrounded = true;
          vel.velocity.y = 0.0f;

          // Precise snap to floor
          float eyeHeight = 1.6f;
          float feetY = transform.position.y - eyeHeight;

          // Use offset to stabilize integer boundary logic
          int blockY = (int)floor(feetY - 0.1f);

          // Snap only if significant deviance? Or always consistent?
          // Consistency is key.
          transform.position.y = (float)(blockY + 1) + eyeHeight;
        } else {
          if (input.isGrounded && vel.velocity.y <= 0) {
            // Walked off ledge?
            input.isGrounded = false;
          }
        }
      }
    }

    // --- PHASE 2: Fluid Physics (Water/Lava) ---
    bool inWater = false;
    bool inLava = false;
    bool headInWater = false;
    bool headInLava = false;
    {
      int ix = (int)floor(transform.position.x);
      int iy = (int)floor(transform.position.y); // Eye pos
      int iz = (int)floor(transform.position.z);

      auto getBlockType = [&](int x, int y, int z) {
        return world.getBlock(x, y, z).getType();
      };

      uint8_t headType = getBlockType(ix, iy, iz);
      if (headType == BlockType::WATER) {
        inWater = true;
        headInWater = true;
      }
      if (headType == BlockType::LAVA) {
        inLava = true;
        headInLava = true;
      }

      // Check feet (Eye - 1.6)
      int iyFeet = (int)floor(transform.position.y - 1.6f);
      uint8_t feetType = getBlockType(ix, iyFeet, iz);
      if (feetType == BlockType::WATER)
        inWater = true;
      if (feetType == BlockType::LAVA)
        inLava = true;

      // Extended Range (Sub-feet) to smooth surface transition.
      // Helps prevent "skipping" by keeping fluid physics active during crest.
      int iySub = (int)floor(transform.position.y - 1.85f);
      uint8_t subType = getBlockType(ix, iySub, iz);
      if (subType == BlockType::WATER)
        inWater = true;
      if (subType == BlockType::LAVA)
        inLava = true;
    }

    // Adjust Gravity & Drag
    if (inLava) {
      // High Viscosity
      gravity.strength = 10.0f;
      vel.velocity *= 0.8f; // Strong drag

      if (vel.velocity.y < -5.0f)
        vel.velocity.y = -5.0f;
      input.isGrounded = false;
    } else if (inWater) {
      // Lower Viscosity but still buoyant
      gravity.strength = 20.0f; // Faster sinking than lava
      vel.velocity *= 0.92f;    // Less drag than lava

      if (vel.velocity.y < -15.0f)
        vel.velocity.y = -15.0f; // Faster terminal velocity
      input.isGrounded = false;
    } else {
      // Air
      gravity.strength = 45.0f;
      float drag = 0.98f;
      vel.velocity.x *= drag;
      vel.velocity.z *= drag;
      // No Y drag from air for now, or very small
    }

    // --- PHASE 3: Input Movement (X/Z) ---
    float speed = input.isSprinting ? input.sprintSpeed : input.movementSpeed;
    float displacement = speed * dt;

    // Slow down in fluids
    if (inLava)
      displacement *= 0.3f;
    else if (inWater)
      displacement *= 0.6f;

    // Vectors
    glm::vec3 front;
    front.x = cos(glm::radians(cam.yaw)) * cos(glm::radians(cam.pitch));
    front.y = sin(glm::radians(cam.pitch));
    front.z = sin(glm::radians(cam.yaw)) * cos(glm::radians(cam.pitch));
    cam.front = glm::normalize(front);
    cam.right = glm::normalize(glm::cross(cam.front, cam.worldUp));
    cam.up = glm::normalize(glm::cross(cam.right, cam.front));

    glm::vec3 flatFront =
        glm::normalize(glm::vec3(cam.front.x, 0.0f, cam.front.z));
    glm::vec3 flatRight =
        glm::normalize(glm::vec3(cam.right.x, 0.0f, cam.right.z));

    glm::vec3 moveDir(0.0f);
    if (forward)
      moveDir += flatFront;
    if (backward)
      moveDir -= flatFront;
    if (left)
      moveDir -= flatRight;
    if (right)
      moveDir += flatRight;

    if (input.flyMode)
      displacement *= 4.0f; // Fly mode overrides fluid slow

    if (glm::length(moveDir) > 0.0f) {
      moveDir = glm::normalize(moveDir);
      glm::vec3 finalMove = moveDir * displacement;

      if (input.flyMode) {
        // Free Fly logic (Noclip)
        glm::vec3 flyDir(0.0f);
        if (forward)
          flyDir += cam.front;
        if (backward)
          flyDir -= cam.front;
        if (left)
          flyDir -= cam.right;
        if (right)
          flyDir += cam.right;
        if (up)
          flyDir += cam.worldUp;
        if (down)
          flyDir -= cam.worldUp;

        if (glm::length(flyDir) > 0.0f) {
          transform.position += glm::normalize(flyDir) * displacement;
        }
        vel.velocity = glm::vec3(0.0f);
      } else {
        // Check X
        glm::vec3 tryX = transform.position;
        tryX.x += finalMove.x;
        if (!checkCollision(tryX))
          transform.position.x = tryX.x;

        // Check Z
        glm::vec3 tryZ = transform.position;
        tryZ.z += finalMove.z;
        tryZ.x = transform.position.x; // Use updated X
        if (!checkCollision(tryZ))
          transform.position.z = tryZ.z;
      }
    }

    // --- PHASE 4: Jump / Swim ---
    // --- PHASE 4: Jump / Swim ---
    if (up) {
      if (headInWater || headInLava) {
        // Only force swim UP if head is submerged.
        // If head is out but feet in, we let buoyancy/gravity handle the
        // bobbing.
        float swimSpeed = 5.0f;
        if (headInLava)
          swimSpeed = 3.0f;
        vel.velocity.y = swimSpeed;
      } else if (inWater || inLava) {
        // Feet in, Head out.
        // Surface Lift Assist: Ensure enough lift to exit fluid.
        // If we sink too deep, we can't get out.
        // Gravity is pulling down.
        float minLift = 3.5f;
        if (inLava)
          minLift = 2.5f; // Slower exit from lava

        // If holding jump at surface, maintain minimum climb speed
        if (vel.velocity.y < minLift) {
          vel.velocity.y = minLift;
        }
      } else if (input.isGrounded) {
        vel.velocity.y = 13.0f;
        input.isGrounded = false;
      }
    }
  });
}

// --- Camera System ---

void CameraSystem::Update(entt::registry &registry, Camera &camera) {
  auto view = registry.view<TransformComponent, CameraComponent>();
  view.each([&camera](auto entity, auto &transform, auto &camComp) {
    // Sync ECS Camera to Global Camera (for rendering)
    camera.Position = transform.position;
    camera.Front = camComp.front;
    camera.Up = camComp.up;
    camera.Yaw = camComp.yaw;
    camera.Pitch = camComp.pitch;
    camera.Zoom = camComp.zoom;
  });
}

// --- Render System ---

void RenderSystem::initCubeMesh() {
  if (cubeVAO != 0)
    return;

  // Pos (3), TexCoord (2)
  float vertices[] = {
      // Back face
      -0.5f, -0.5f, -0.5f, 0.0f, 0.0f, 0.5f, 0.5f, -0.5f, 1.0f, 1.0f, 0.5f,
      -0.5f, -0.5f, 1.0f, 0.0f, 0.5f, 0.5f, -0.5f, 1.0f, 1.0f, -0.5f, -0.5f,
      -0.5f, 0.0f, 0.0f, -0.5f, 0.5f, -0.5f, 0.0f, 1.0f,

      // Front face
      -0.5f, -0.5f, 0.5f, 0.0f, 0.0f, 0.5f, -0.5f, 0.5f, 1.0f, 0.0f, 0.5f, 0.5f,
      0.5f, 1.0f, 1.0f, 0.5f, 0.5f, 0.5f, 1.0f, 1.0f, -0.5f, 0.5f, 0.5f, 0.0f,
      1.0f, -0.5f, -0.5f, 0.5f, 0.0f, 0.0f,

      // Left face
      -0.5f, 0.5f, 0.5f, 1.0f, 0.0f, -0.5f, 0.5f, -0.5f, 1.0f, 1.0f, -0.5f,
      -0.5f, -0.5f, 0.0f, 1.0f, -0.5f, -0.5f, -0.5f, 0.0f, 1.0f, -0.5f, -0.5f,
      0.5f, 0.0f, 0.0f, -0.5f, 0.5f, 0.5f, 1.0f, 0.0f,

      // Right face
      0.5f, 0.5f, 0.5f, 1.0f, 0.0f, 0.5f, -0.5f, -0.5f, 0.0f, 1.0f, 0.5f, 0.5f,
      -0.5f, 1.0f, 1.0f, 0.5f, -0.5f, -0.5f, 0.0f, 1.0f, 0.5f, 0.5f, 0.5f, 1.0f,
      0.0f, 0.5f, -0.5f, 0.5f, 0.0f, 0.0f,

      // Bottom face
      -0.5f, -0.5f, -0.5f, 0.0f, 1.0f, 0.5f, -0.5f, -0.5f, 1.0f, 1.0f, 0.5f,
      -0.5f, 0.5f, 1.0f, 0.0f, 0.5f, -0.5f, 0.5f, 1.0f, 0.0f, -0.5f, -0.5f,
      0.5f, 0.0f, 0.0f, -0.5f, -0.5f, -0.5f, 0.0f, 1.0f,

      // Top face
      -0.5f, 0.5f, -0.5f, 0.0f, 1.0f, 0.5f, 0.5f, 0.5f, 1.0f, 0.0f, 0.5f, 0.5f,
      -0.5f, 1.0f, 1.0f, 0.5f, 0.5f, 0.5f, 1.0f, 0.0f, -0.5f, 0.5f, 0.5f, 0.0f,
      0.0f, -0.5f, 0.5f, -0.5f, 0.0f, 1.0f};

  glGenVertexArrays(1, &cubeVAO);
  glGenBuffers(1, &cubeVBO);

  glBindBuffer(GL_ARRAY_BUFFER, cubeVBO);
  glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

  glBindVertexArray(cubeVAO);

  // vec3 aPos (Loc 0)
  glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void *)0);
  glEnableVertexAttribArray(0);

  // vec2 aTexCoord (Loc 2) - Note: Shader expects loc 2 for tex coord
  glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float),
                        (void *)(3 * sizeof(float)));
  glEnableVertexAttribArray(2);

  // Disable others to rely on static values
  glDisableVertexAttribArray(1); // aColor
  glDisableVertexAttribArray(3); // aLight
  glDisableVertexAttribArray(4); // aTexOrigin
}

void RenderSystem::Render(entt::registry &registry, World &world,
                          Shader &shader, const glm::mat4 &viewProjection) {
  initCubeMesh();

  auto view = registry.view<TransformComponent, BlockComponent>();

  glBindVertexArray(cubeVAO);

  // Set default attributes needed by shader
  // aColor (Loc 1) = White
  glVertexAttrib4f(1, 1.0f, 1.0f, 1.0f, 1.0f);

  // aLight (Loc 3) - We will update this per entity

  view.each([&shader, &world](auto entity, auto &transform, auto &blockComp) {
    glm::mat4 model = glm::mat4(1.0f);
    model = glm::translate(model, transform.position);
    model = glm::scale(model, transform.scale);

    shader.setMat4("model", model);

    // Setup Texture Origin
    Block *block = BlockRegistry::getInstance().getBlock(blockComp.type);
    float u, v;
    block->getTextureUV(2, u, v);
    glVertexAttrib2f(4, u, v);

    // Sample Light
    // Sample at the center of the entity
    int x = std::floor(transform.position.x);
    int y = std::floor(transform.position.y);
    int z = std::floor(transform.position.z);

    ChunkBlock b = world.getBlock(x, y, z);
    float sun = b.skyLight / 15.0f;
    float blk = b.blockLight / 15.0f;

    // Use aLight uniform logic directly
    glVertexAttrib3f(3, sun, blk, 0.0f); // AO = 0 (full brightness)

    glDrawArrays(GL_TRIANGLES, 0, 36);
  });
  // Cleanup? Restore state?
  // Not strictly necessary if main loop resets VAOs, but good practice to
  // enable arrays if needed.
}
