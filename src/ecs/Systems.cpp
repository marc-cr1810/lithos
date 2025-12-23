#include "Systems.h"
#include "Components.h"
#include "../world/World.h"
#include <iostream>
#include <glm/gtc/matrix_transform.hpp>

// --- Physics System ---

void PhysicsSystem::Update(entt::registry& registry, float dt) {
    auto view = registry.view<TransformComponent, VelocityComponent, GravityComponent>();

    view.each([dt](auto entity, auto& transform, auto& vel, auto& gravity) {
        // Apply Gravity
        vel.velocity.y -= gravity.strength * dt;

        // Apply Velocity (Collision system will correct this later ideally, but for now apply first)
        transform.position += vel.velocity * dt;
    });
}

// --- Collision System ---

void CollisionSystem::Update(entt::registry& registry, World& world, float dt) {
    auto view = registry.view<TransformComponent, VelocityComponent, ColliderComponent, BlockComponent>();

    view.each([&world, dt, &registry](auto entity, auto& transform, auto& vel, auto& collider, auto& block) {
        // Simple AABB vs Voxel collision
        // Check center point + offsets?
        
        // Check if bottom center is inside a solid block
        // Transform position is center of entity
        glm::vec3 bottomPoint = transform.position - glm::vec3(0, collider.size.y * 0.5f, 0);
        glm::vec3 checkPos = bottomPoint + glm::vec3(0, -0.05f, 0); // Check slightly below

        int bx = std::floor(checkPos.x);
        int by = std::floor(checkPos.y);
        int bz = std::floor(checkPos.z);

        ChunkBlock b = world.getBlock(bx, by, bz);
        if (b.getType() != BlockType::AIR && b.getType() != BlockType::WATER && b.getType() != BlockType::LAVA) {
            // Collision with ground
            if (vel.velocity.y < 0) {
                // Stop
                vel.velocity = glm::vec3(0);

                // Re-solidify
                // Snap to nearest block integer
                int ix = std::floor(transform.position.x);
                int iy = std::floor(transform.position.y); // Use center Y, but might need to offset? 
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

void RenderSystem::initCubeMesh() {
    if (cubeVAO != 0) return;

    // Pos (3), TexCoord (2)
    float vertices[] = {
        // Back face
        -0.5f, -0.5f, -0.5f,  0.0f, 0.0f,
         0.5f,  0.5f, -0.5f,  1.0f, 1.0f,
         0.5f, -0.5f, -0.5f,  1.0f, 0.0f,
         0.5f,  0.5f, -0.5f,  1.0f, 1.0f,
        -0.5f, -0.5f, -0.5f,  0.0f, 0.0f,
        -0.5f,  0.5f, -0.5f,  0.0f, 1.0f,

        // Front face
        -0.5f, -0.5f,  0.5f,  0.0f, 0.0f,
         0.5f, -0.5f,  0.5f,  1.0f, 0.0f,
         0.5f,  0.5f,  0.5f,  1.0f, 1.0f,
         0.5f,  0.5f,  0.5f,  1.0f, 1.0f,
        -0.5f,  0.5f,  0.5f,  0.0f, 1.0f,
        -0.5f, -0.5f,  0.5f,  0.0f, 0.0f,

        // Left face
        -0.5f,  0.5f,  0.5f,  1.0f, 0.0f,
        -0.5f,  0.5f, -0.5f,  1.0f, 1.0f,
        -0.5f, -0.5f, -0.5f,  0.0f, 1.0f,
        -0.5f, -0.5f, -0.5f,  0.0f, 1.0f,
        -0.5f, -0.5f,  0.5f,  0.0f, 0.0f,
        -0.5f,  0.5f,  0.5f,  1.0f, 0.0f,

        // Right face
         0.5f,  0.5f,  0.5f,  1.0f, 0.0f,
         0.5f, -0.5f, -0.5f,  0.0f, 1.0f,
         0.5f,  0.5f, -0.5f,  1.0f, 1.0f,
         0.5f, -0.5f, -0.5f,  0.0f, 1.0f,
         0.5f,  0.5f,  0.5f,  1.0f, 0.0f,
         0.5f, -0.5f,  0.5f,  0.0f, 0.0f,

        // Bottom face
        -0.5f, -0.5f, -0.5f,  0.0f, 1.0f,
         0.5f, -0.5f, -0.5f,  1.0f, 1.0f,
         0.5f, -0.5f,  0.5f,  1.0f, 0.0f,
         0.5f, -0.5f,  0.5f,  1.0f, 0.0f,
        -0.5f, -0.5f,  0.5f,  0.0f, 0.0f,
        -0.5f, -0.5f, -0.5f,  0.0f, 1.0f,

        // Top face
        -0.5f,  0.5f, -0.5f,  0.0f, 1.0f,
         0.5f,  0.5f,  0.5f,  1.0f, 0.0f,
         0.5f,  0.5f, -0.5f,  1.0f, 1.0f,
         0.5f,  0.5f,  0.5f,  1.0f, 0.0f,
        -0.5f,  0.5f,  0.5f,  0.0f, 0.0f,
        -0.5f,  0.5f, -0.5f,  0.0f, 1.0f
    };

    glGenVertexArrays(1, &cubeVAO);
    glGenBuffers(1, &cubeVBO);

    glBindBuffer(GL_ARRAY_BUFFER, cubeVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    glBindVertexArray(cubeVAO);

    // vec3 aPos (Loc 0)
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    
    // vec2 aTexCoord (Loc 2) - Note: Shader expects loc 2 for tex coord
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(2);
    
    // Disable others to rely on static values
    glDisableVertexAttribArray(1); // aColor
    glDisableVertexAttribArray(3); // aLight
    glDisableVertexAttribArray(4); // aTexOrigin
}

void RenderSystem::Render(entt::registry& registry, World& world, Shader& shader, const glm::mat4& viewProjection) {
    initCubeMesh();

    auto view = registry.view<TransformComponent, BlockComponent>();
    
    glBindVertexArray(cubeVAO);
    
    // Set default attributes needed by shader
    // aColor (Loc 1) = White
    glVertexAttrib4f(1, 1.0f, 1.0f, 1.0f, 1.0f);
    
    // aLight (Loc 3) - We will update this per entity
    
    view.each([&shader, &world](auto entity, auto& transform, auto& blockComp) {
        glm::mat4 model = glm::mat4(1.0f);
        model = glm::translate(model, transform.position);
        model = glm::scale(model, transform.scale);
        
        shader.setMat4("model", model);
        
        // Setup Texture Origin
        Block* block = BlockRegistry::getInstance().getBlock(blockComp.type);
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
    // Not strictly necessary if main loop resets VAOs, but good practice to enable arrays if needed.
}
