#include "Block.h"
#include "blocks/AirBlock.h"
#include "blocks/SolidBlock.h"
#include "blocks/LiquidBlock.h"
#include "blocks/PlantBlock.h"
#include "blocks/LightBlock.h"
#include "blocks/FallingBlock.h"

BlockRegistry& BlockRegistry::getInstance() {
    static BlockRegistry instance;
    return instance;
}

BlockRegistry::BlockRegistry() {
    // Default to Air to avoid crashes
    defaultBlock = new AirBlock();
    blocks[BlockType::AIR] = defaultBlock;

    registerBlock(new AirBlock());
    registerBlock(new SolidBlock(BlockType::DIRT, "Dirt"));
    registerBlock(new SolidBlock(BlockType::GRASS, "Grass"));
    registerBlock(new SolidBlock(BlockType::STONE, "Stone"));
    registerBlock(new SolidBlock(BlockType::WOOD, "Wood"));
    registerBlock(new PlantBlock(BlockType::LEAVES, "Leaves"));
    registerBlock(new SolidBlock(BlockType::COAL_ORE, "Coal Ore"));
    registerBlock(new SolidBlock(BlockType::IRON_ORE, "Iron Ore"));
    registerBlock(new LightBlock(BlockType::GLOWSTONE, "Glowstone", 15));
    registerBlock(new LiquidBlock(BlockType::WATER, "Water"));
    registerBlock(new LiquidBlock(BlockType::LAVA, "Lava"));
    registerBlock(new FallingBlock(BlockType::SAND, "Sand"));
    registerBlock(new FallingBlock(BlockType::GRAVEL, "Gravel"));
}

void BlockRegistry::registerBlock(Block* block) {
    blocks[block->getId()] = block;
}

Block* BlockRegistry::getBlock(uint8_t id) {
    auto it = blocks.find(id);
    if (it != blocks.end()) {
        return it->second;
    }
    return defaultBlock;
}

BlockRegistry::~BlockRegistry() {
    // Cleanup
}
