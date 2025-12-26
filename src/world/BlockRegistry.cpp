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

  Block *dirt = new SolidBlock(BlockType::DIRT, "Dirt");
  dirt->setTexture("dirt");
  registerBlock(dirt);

  Block *grass = new SolidBlock(BlockType::GRASS, "Grass");
  grass->setTexture("dirt"); // Default/Bottom
  grass->setTexture(4, "grass_block_top");
  grass->setTexture(0, "grass_block_side");
  grass->setTexture(1, "grass_block_side");
  grass->setTexture(2, "grass_block_side");
  grass->setTexture(3, "grass_block_side");
  grass->setOverlayTexture(0, "grass_block_side_overlay");
  grass->setOverlayTexture(1, "grass_block_side_overlay");
  grass->setOverlayTexture(2, "grass_block_side_overlay");
  grass->setOverlayTexture(3, "grass_block_side_overlay");
  registerBlock(grass);

  Block *stone = new SolidBlock(BlockType::STONE, "Stone");
  stone->setTexture("stone");
  registerBlock(stone);

  Block *wood = new SolidBlock(BlockType::WOOD, "Wood");
  wood->setTexture("oak_log");
  wood->setTexture(4, "oak_log_top");
  wood->setTexture(5, "oak_log_top");
  registerBlock(wood);

  Block *leaves = new PlantBlock(BlockType::LEAVES, "Leaves");
  leaves->setTexture("oak_leaves");
  registerBlock(leaves);

  Block *coal = new SolidBlock(BlockType::COAL_ORE, "Coal Ore");
  coal->setTexture("coal_ore");
  registerBlock(coal);

  Block *iron = new SolidBlock(BlockType::IRON_ORE, "Iron Ore");
  iron->setTexture("iron_ore");
  registerBlock(iron);

  Block *glow = new LightBlock(BlockType::GLOWSTONE, "Glowstone", 15);
  glow->setTexture("glowstone");
  registerBlock(glow);

  Block *water = new LiquidBlock(BlockType::WATER, "Water");
  water->setTexture(
      "water_still"); // Use still for all for now, maybe flow for sides?
  // water->setTexture(0, "water_flow"); ...
  // Let's stick to water_still to match expectations or simple setup
  registerBlock(water);

  Block *lava = new LiquidBlock(BlockType::LAVA, "Lava");
  lava->setTexture("lava_still");
  registerBlock(lava);

  Block *sand = new FallingBlock(BlockType::SAND, "Sand");
  sand->setTexture("sand");
  registerBlock(sand);

  Block *gravel = new FallingBlock(BlockType::GRAVEL, "Gravel");
  gravel->setTexture("gravel");
  registerBlock(gravel);

  Block *snow = new SolidBlock(BlockType::SNOW, "Snow");
  snow->setTexture("snow");
  registerBlock(snow);

  Block *ice = new SolidBlock(BlockType::ICE, "Ice");
  ice->setTexture("ice");
  registerBlock(ice);

  // Flora & Structures
  Block *cactus = new SolidBlock(BlockType::CACTUS, "Cactus");
  cactus->setTexture("cactus_side");
  cactus->setTexture(4, "cactus_top");
  cactus->setTexture(5, "cactus_bottom"); // Bottom
  registerBlock(cactus);

  Block *pine = new SolidBlock(BlockType::PINE_WOOD, "Pine Wood");
  pine->setTexture("spruce_log");
  pine->setTexture(4, "spruce_log_top");
  pine->setTexture(5, "spruce_log_top");
  registerBlock(pine);

  Block *pineLeaves = new PlantBlock(BlockType::PINE_LEAVES, "Pine Leaves");
  pineLeaves->setTexture("spruce_leaves");
  registerBlock(pineLeaves);

  Block *tallGrass = new PlantBlock(BlockType::TALL_GRASS, "Tall Grass");
  tallGrass->setTexture("short_grass");
  registerBlock(tallGrass);

  Block *deadBush = new PlantBlock(BlockType::DEAD_BUSH, "Dead Bush");
  deadBush->setTexture("dead_bush");
  registerBlock(deadBush);

  Block *rose = new PlantBlock(BlockType::ROSE, "Rose");
  rose->setTexture("poppy");
  registerBlock(rose);

  Block *dryShort =
      new PlantBlock(BlockType::DRY_SHORT_GRASS, "Dry Short Grass");
  dryShort->setTexture("short_dry_grass");
  registerBlock(dryShort);

  Block *dryTall = new PlantBlock(BlockType::DRY_TALL_GRASS, "Dry Tall Grass");
  dryTall->setTexture("tall_dry_grass");
  registerBlock(dryTall);
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
