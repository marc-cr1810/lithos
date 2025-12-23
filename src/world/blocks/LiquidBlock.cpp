#include "LiquidBlock.h"
#include "../World.h"

// Metadata: 0 = Source/Full Strength, 1-7 = Decaying Flow

void LiquidBlock::onPlace(World& world, int x, int y, int z) const {
    // Schedule immediate update to start flow
    world.scheduleBlockUpdate(x, y, z, (id == WATER) ? 5 : 30);
}

void LiquidBlock::onNeighborChange(World& world, int x, int y, int z, int nx, int ny, int nz) const {
    // If neighbor changed (air -> solid or solid -> air), we need to re-evaluate
    // Don't spam updates, but ensure we check.
    world.scheduleBlockUpdate(x, y, z, (id == WATER) ? 5 : 30);
}

void LiquidBlock::update(World& world, int x, int y, int z) const {
    uint8_t meta = world.getMetadata(x, y, z);
    
    // Check if we are still supported (Decay Logic)
    // If not source (meta != 0), check if we have a valid parent
    // Parent = Water block above OR Water block on side with lower meta
    if(meta != 0) {
        bool usersource = false;
        
        // Check Above
        ChunkBlock above = world.getBlock(x, y + 1, z);
        if(above.isActive() && above.block->getId() == this->id) {
            usersource = true;
        } else {
            // Check Sides
            int dirs[4][2] = {{1,0}, {-1,0}, {0,1}, {0,-1}};
            for(auto& d : dirs) {
                ChunkBlock nb = world.getBlock(x + d[0], y, z + d[1]);
                if(nb.isActive() && nb.block->getId() == this->id) {
                    uint8_t nMeta = world.getMetadata(x + d[0], y, z + d[1]);
                    if(nMeta < meta) {
                        usersource = true;
                        break;
                    }
                }
            }
        }
        
        if(!usersource) {
            // Decay
            world.setBlock(x, y, z, AIR);
            world.setMetadata(x, y, z, 0);
            return; // Stop processing
        }
    }

    // Spread Down
    ChunkBlock below = world.getBlock(x, y - 1, z);
    bool canFlowDown = !below.isActive() || (!below.isSolid() && below.block->getId() != this->id);
    
    if(canFlowDown) {
        // Reset strength when falling
        trySpread(world, x, y - 1, z, 0); 
        return; // Don't spread sides if falling (unless supported? varying logic)
        // MC Logic: If falling, it doesn't spread sides from the falling stream itself, 
        // BUT the block *above* it is the one doing the logic.
        // Wait, "THIS" block is the one updating.
        // If I flowed down, do I also flow sides?
        // Usually: Prefer down. If down is possible, don't flow sides.
        // If down is blocked (or existing water), flow sides.
    }
    
    // If block below is liquid of same type, we treat it as "blocked" for flow purposes (it's already full)
    // But we might want to continue updating it?
    
    // Spread Sides
    if(meta < 7) {
        int dirs[4][2] = {{1,0}, {-1,0}, {0,1}, {0,-1}};
        for(auto& d : dirs) {
            trySpread(world, x + d[0], y, z + d[1], meta + 1);
        }
    }
}

void LiquidBlock::trySpread(World& world, int x, int y, int z, int newMeta) const {
    ChunkBlock b = world.getBlock(x, y, z);
    
    // Replace Air or Non-Solid/Vegetation
    // Also replacing water with higher meta (stronger flow replaces weaker)?
    // For now: only replace air/different blocks
    if(!b.isActive() || (!b.isSolid() && b.block->getId() != this->id)) {
        world.setBlock(x, y, z, (BlockType)this->id);
        world.setMetadata(x, y, z, newMeta);
        
        // Schedule next update for the new block
        // Water: 5 ticks, Lava: 30 ticks
        int delay = (this->id == WATER) ? 5 : 30;
        world.scheduleBlockUpdate(x, y, z, delay);
    }
    // Optimization: If it IS water but higher meta, update it?
    else if(b.isActive() && b.block->getId() == this->id) {
        uint8_t currentMeta = world.getMetadata(x, y, z);
        if(newMeta < currentMeta) {
             world.setMetadata(x, y, z, newMeta);
             int delay = (this->id == WATER) ? 5 : 30;
             world.scheduleBlockUpdate(x, y, z, delay);
        }
    }
}
