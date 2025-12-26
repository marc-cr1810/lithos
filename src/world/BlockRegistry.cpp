#include "Block.h"
#include "blocks/AirBlock.h"
#include "blocks/FallingBlock.h"
#include "blocks/LightBlock.h"
#include "blocks/LiquidBlock.h"
#include "blocks/PlantBlock.h"
#include "blocks/SolidBlock.h"

BlockRegistry &BlockRegistry::getInstance() {
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
  registerBlock(new SolidBlock(BlockType::SNOW, "Snow"));
  registerBlock(new SolidBlock(BlockType::ICE, "Ice"));

  // Flora & Structures
  registerBlock(new SolidBlock(BlockType::CACTUS, "Cactus"));
  registerBlock(new SolidBlock(BlockType::PINE_WOOD, "Pine Wood"));
  registerBlock(new PlantBlock(BlockType::PINE_LEAVES, "Pine Leaves"));
  registerBlock(new PlantBlock(BlockType::TALL_GRASS, "Tall Grass"));
  registerBlock(new PlantBlock(BlockType::DEAD_BUSH, "Dead Bush"));
  registerBlock(new PlantBlock(BlockType::ROSE, "Rose"));
}

void BlockRegistry::registerBlock(Block *block) {
  blocks[block->getId()] = block;
}

Block *BlockRegistry::getBlock(uint8_t id) {
  auto it = blocks.find(id);
  if (it != blocks.end()) {
    return it->second;
  }
  return defaultBlock;
}

BlockRegistry::~BlockRegistry() {
  // Cleanup
}
