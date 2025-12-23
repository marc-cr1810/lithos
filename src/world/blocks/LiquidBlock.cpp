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
    
    // Check if we can flow down into the block below
    // 1. It is Air/Reconfirm
    // 2. It is Non-Solid and Different ID (e.g. washing away grass)
    // 3. SPECIAL: It is SAME ID (Water on Water).
    //    - If water below is "supported" by a solid block, we treat 'below' as a floor -> Blocked -> Spread sides.
    //    - If water below is NOT supported (Air/Water below it), we treat 'below' as a column -> Flow down (Merge) -> Don't spread sides.
    
    bool canFlowDown = !below.isActive() || (!below.isSolid() && below.block->getId() != this->id);
    
    if(!canFlowDown && below.isActive() && below.block->getId() == this->id) {
        // Below is same liquid. Check support.
        ChunkBlock below2 = world.getBlock(x, y - 2, z);
        if(below2.isActive() && below2.isSolid()) {
            // Supported -> effectively blocked -> don't flow down (allow spread)
            canFlowDown = false;
        } else {
            // Not supported (Air or Water below) -> flow down (merge)
            canFlowDown = true;
        }
    }
    
    if(canFlowDown) {
        // Reset strength when falling
        trySpread(world, x, y - 1, z, 0); 
        return; 
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
    // FIX: Do NOT replace other liquids (Water vs Lava)
    if(!b.isActive() || (!b.isSolid() && b.block->getId() != this->id && b.block->getId() != WATER && b.block->getId() != LAVA)) {
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
