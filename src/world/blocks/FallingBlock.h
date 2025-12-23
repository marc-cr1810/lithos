#ifndef FALLING_BLOCK_H
#define FALLING_BLOCK_H

#include "SolidBlock.h"
#include "../World.h"
#include "../../ecs/Components.h"
#include <iostream>

class FallingBlock : public SolidBlock {
public:
    FallingBlock(uint8_t id, const std::string& name) : SolidBlock(id, name) {}

    void onPlace(World& world, int x, int y, int z) const override {
        world.scheduleBlockUpdate(x, y, z, 2); 
    }

    void onNeighborChange(World& world, int x, int y, int z, int nx, int ny, int nz) const override {
        if (nx == x && ny == y - 1 && nz == z) {
            world.scheduleBlockUpdate(x, y, z, 2);
        }
    }

    void update(World& world, int x, int y, int z) const override {
        ChunkBlock below = world.getBlock(x, y - 1, z);
        if (canFallThrough(below)) {
            // Remove block
            world.setBlock(x, y, z, AIR);
            
            // Spawn Entity
            auto entity = world.registry.create();
            world.registry.emplace<TransformComponent>(entity, glm::vec3(x + 0.5f, y + 0.5f, z + 0.5f), glm::vec3(0), glm::vec3(1.0f));
            world.registry.emplace<VelocityComponent>(entity, glm::vec3(0, 0, 0));
            world.registry.emplace<GravityComponent>(entity, 20.0f); // Fast gravity
            world.registry.emplace<ColliderComponent>(entity, glm::vec3(0.98f));
            world.registry.emplace<BlockComponent>(entity, (BlockType)id);
        }
    }

private:
    bool canFallThrough(const ChunkBlock& b) const {
        return b.getType() == AIR || b.getType() == WATER || b.getType() == LAVA;
    }
};

#endif
