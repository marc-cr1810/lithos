#include "Block.h"
#include "blocks/AirBlock.h"
#include "blocks/FallingBlock.h"
#include "blocks/LayeredBlock.h"
#include "blocks/LightBlock.h"
#include "blocks/LiquidBlock.h"
#include "blocks/LogBlock.h"
#include "blocks/MetadataBlock.h"
#include "blocks/PlantBlock.h"
#include "blocks/SlabBlock.h"
#include "blocks/SolidBlock.h"
#include "blocks/StairBlock.h"
#include "debug/Logger.h"
#include <filesystem>
#include <iostream>

BlockRegistry &BlockRegistry::getInstance() {
  static BlockRegistry instance;
  return instance;
}

BlockRegistry::BlockRegistry() {
  // Default to Air to avoid crashes
  defaultBlock = new AirBlock();
  blocks[BlockType::AIR] = defaultBlock;

  registerBlock(new AirBlock()); // Air doesn't strictly need ID if it's
                                 // default? Or "lithos:air"

  Block *dirt = new SolidBlock(BlockType::DIRT, "Dirt");
  dirt->setResourceId("lithos:dirt");
  dirt->setTexture("soil/dirt");
  registerBlock(dirt);

  Block *grass = new SolidBlock(BlockType::GRASS, "Grass");
  grass->setResourceId("lithos:grass");
  grass->setTexture("soil/dirt"); // Default/Bottom
  grass->setTexture(4, "soil/grass_block_top");
  grass->setTexture(0, "soil/grass_block_side");
  grass->setTexture(1, "soil/grass_block_side");
  grass->setTexture(2, "soil/grass_block_side");
  grass->setTexture(3, "soil/grass_block_side");
  grass->setOverlayTexture(0, "soil/grass_block_side_overlay");
  grass->setOverlayTexture(1, "soil/grass_block_side_overlay");
  grass->setOverlayTexture(2, "soil/grass_block_side_overlay");
  grass->setOverlayTexture(3, "soil/grass_block_side_overlay");
  registerBlock(grass);

  Block *stone = new SolidBlock(BlockType::STONE, "Stone");
  stone->setResourceId("lithos:stone");
  stone->setTexture("stone/rock/stone");
  registerBlock(stone);

  Block *wood = new LogBlock(BlockType::WOOD, "Oak Log");
  wood->setResourceId("lithos:oak_log");
  wood->setTexture("wood/bark/oak_log");
  wood->setTexture(4, "wood/treetrunk/oak_log_top");
  wood->setTexture(5, "wood/treetrunk/oak_log_top");
  registerBlock(wood);

  Block *leaves = new PlantBlock(BlockType::LEAVES, "Oak Leaves");
  leaves->setResourceId("lithos:oak_leaves");
  leaves->setTexture("plant/leaves/oak_leaves");
  registerBlock(leaves);

  Block *coal = new SolidBlock(BlockType::COAL_ORE, "Coal Ore");
  coal->setResourceId("lithos:coal_ore");
  coal->setTexture("stone/ore/coal_ore");
  registerBlock(coal);

  Block *iron = new SolidBlock(BlockType::IRON_ORE, "Iron Ore");
  iron->setResourceId("lithos:iron_ore");
  iron->setTexture("stone/ore/iron_ore");
  registerBlock(iron);

  Block *glow = new LightBlock(BlockType::GLOWSTONE, "Glowstone", 15);
  glow->setResourceId("lithos:glowstone");
  glow->setTexture("stone/rock/glowstone");
  registerBlock(glow);

  Block *water = new LiquidBlock(BlockType::WATER, "Water");
  water->setResourceId("lithos:water");
  water->setTexture("liquid/water_still");
  water->setTexture(0, "liquid/water_flow");
  water->setTexture(1, "liquid/water_flow");
  water->setTexture(2, "liquid/water_flow");
  water->setTexture(3, "liquid/water_flow");
  registerBlock(water);

  Block *lava = new LiquidBlock(BlockType::LAVA, "Lava");
  lava->setResourceId("lithos:lava");
  lava->setTexture("liquid/lava_still");
  lava->setTexture(0, "liquid/lava_flow");
  lava->setTexture(1, "liquid/lava_flow");
  lava->setTexture(2, "liquid/lava_flow");
  lava->setTexture(3, "liquid/lava_flow");
  registerBlock(lava);

  Block *sand = new FallingBlock(BlockType::SAND, "Sand");
  sand->setResourceId("lithos:sand");
  sand->setTexture("stone/sand/sand");
  registerBlock(sand);

  Block *gravel = new FallingBlock(BlockType::GRAVEL, "Gravel");
  gravel->setResourceId("lithos:gravel");
  gravel->setTexture("stone/gravel/gravel");
  registerBlock(gravel);

  Block *snow = new SolidBlock(BlockType::SNOW, "Snow");
  snow->setResourceId("lithos:snow");
  snow->setTexture("liquid/snow");
  registerBlock(snow);

  Block *ice = new SolidBlock(BlockType::ICE, "Ice");
  ice->setResourceId("lithos:ice");
  ice->setTexture("liquid/ice");
  ice->setOpaque(false);
  registerBlock(ice);

  // Flora & Structures
  Block *cactus = new SolidBlock(BlockType::CACTUS, "Cactus");
  cactus->setResourceId("lithos:cactus");
  cactus->setTexture("plant/cactus/cactus_side");
  cactus->setTexture(4, "plant/cactus/cactus_top");
  cactus->setTexture(5, "plant/cactus/cactus_bottom");
  cactus->setOpaque(false);
  registerBlock(cactus);

  Block *spruceLog = new LogBlock(BlockType::SPRUCE_LOG, "Spruce Log");
  spruceLog->setResourceId("lithos:spruce_log");
  spruceLog->setTexture("wood/bark/spruce_log");
  spruceLog->setTexture(4, "wood/treetrunk/spruce_log_top");
  spruceLog->setTexture(5, "wood/treetrunk/spruce_log_top");
  // Model loading will handle the rest if file exists
  registerBlock(spruceLog);

  Block *spruceLeaves =
      new PlantBlock(BlockType::SPRUCE_LEAVES, "Spruce Leaves");
  spruceLeaves->setResourceId("lithos:spruce_leaves");
  spruceLeaves->setTexture("plant/leaves/spruce_leaves");
  registerBlock(spruceLeaves);

  Block *tallGrass = new PlantBlock(BlockType::TALL_GRASS, "Tall Grass");
  tallGrass->setResourceId("lithos:tall_grass");
  tallGrass->setTexture("plant/tallgrass/short_grass");
  registerBlock(tallGrass);

  Block *deadBush = new PlantBlock(BlockType::DEAD_BUSH, "Dead Bush");
  deadBush->setResourceId("lithos:dead_bush");
  deadBush->setTexture("plant/tallgrass/dead_bush");
  registerBlock(deadBush);

  Block *rose = new PlantBlock(BlockType::ROSE, "Rose");
  rose->setResourceId("lithos:rose");
  rose->setTexture("plant/flower/poppy");
  registerBlock(rose);

  Block *dryShort =
      new PlantBlock(BlockType::DRY_SHORT_GRASS, "Dry Short Grass");
  dryShort->setResourceId("lithos:dry_short_grass");
  dryShort->setTexture("plant/tallgrass/short_dry_grass");
  registerBlock(dryShort);

  Block *dryTall = new PlantBlock(BlockType::DRY_TALL_GRASS, "Dry Tall Grass");
  dryTall->setResourceId("lithos:dry_tall_grass");
  dryTall->setTexture("plant/tallgrass/tall_dry_grass");
  registerBlock(dryTall);

  // New blocks
  Block *obsidian = new SolidBlock(BlockType::OBSIDIAN, "Obsidian");
  obsidian->setResourceId("lithos:obsidian");
  obsidian->setTexture("stone/rock/obsidian");
  registerBlock(obsidian);

  Block *cobblestone = new SolidBlock(BlockType::COBBLESTONE, "Cobblestone");
  cobblestone->setResourceId("lithos:cobblestone");
  cobblestone->setTexture("stone/rock/cobblestone");
  registerBlock(cobblestone);

  // Wood planks with metadata support: 0 = oak, 1 = spruce
  MetadataBlock *woodPlanks =
      new MetadataBlock(BlockType::WOOD_PLANKS, "Wood Planks");
  woodPlanks->setResourceId("lithos:planks"); // Or separate
  woodPlanks->setTextureForMetadata(0, "wood/planks/oak_planks");
  woodPlanks->setTextureForMetadata(1, "wood/planks/spruce_planks");
  registerBlock((Block *)woodPlanks);

  // Custom Mesh Blocks
  Block *stoneSlab = new SlabBlock(BlockType::STONE_SLAB, "Stone Slab");
  stoneSlab->setResourceId("lithos:stone_slab");
  stoneSlab->setTexture("stone/rock/stone");
  registerBlock(stoneSlab);

  Block *woodStairs = new StairBlock(BlockType::WOOD_STAIRS, "Oak Stairs");
  woodStairs->setResourceId("lithos:oak_stairs");
  woodStairs->setTexture("wood/planks/oak_planks");
  registerBlock(woodStairs);

  // Geological Blocks
  Block *andesite = new SolidBlock(BlockType::ANDESITE, "Andesite");
  andesite->setResourceId("lithos:andesite");
  andesite->setTexture("stone/rock/andesite");
  registerBlock(andesite);

  Block *basalt = new SolidBlock(BlockType::BASALT, "Basalt");
  basalt->setResourceId("lithos:basalt");
  basalt->setTexture("stone/rock/basalt_side");
  basalt->setTexture(4, "stone/rock/basalt_top");
  basalt->setTexture(5, "stone/rock/basalt_top");
  registerBlock(basalt);

  Block *diorite = new SolidBlock(BlockType::DIORITE, "Diorite");
  diorite->setResourceId("lithos:diorite");
  diorite->setTexture("stone/rock/diorite");
  registerBlock(diorite);

  Block *granite = new SolidBlock(BlockType::GRANITE, "Granite");
  granite->setResourceId("lithos:granite");
  granite->setTexture("stone/rock/granite");
  registerBlock(granite);

  // New Logs
  Block *acaciaLog = new LogBlock(BlockType::ACACIA_LOG, "Acacia Log");
  acaciaLog->setResourceId("lithos:acacia_log");
  acaciaLog->setTexture("wood/bark/acacia_log");
  acaciaLog->setTexture(4, "wood/treetrunk/acacia_log_top");
  acaciaLog->setTexture(5, "wood/treetrunk/acacia_log_top");
  registerBlock(acaciaLog);

  Block *birchLog = new LogBlock(BlockType::BIRCH_LOG, "Birch Log");
  birchLog->setResourceId("lithos:birch_log");
  birchLog->setTexture("wood/bark/birch_log");
  birchLog->setTexture(4, "wood/treetrunk/birch_log_top");
  birchLog->setTexture(5, "wood/treetrunk/birch_log_top");
  registerBlock(birchLog);

  Block *darkOakLog = new LogBlock(BlockType::DARK_OAK_LOG, "Dark Oak Log");
  darkOakLog->setResourceId("lithos:dark_oak_log");
  darkOakLog->setTexture("wood/bark/dark_oak_log");
  darkOakLog->setTexture(4, "wood/treetrunk/dark_oak_log_top");
  darkOakLog->setTexture(5, "wood/treetrunk/dark_oak_log_top");
  registerBlock(darkOakLog);

  Block *jungleLog = new LogBlock(BlockType::JUNGLE_LOG, "Jungle Log");
  jungleLog->setResourceId("lithos:jungle_log");
  jungleLog->setTexture("wood/bark/jungle_log");
  jungleLog->setTexture(4, "wood/treetrunk/jungle_log_top");
  jungleLog->setTexture(5, "wood/treetrunk/jungle_log_top");
  registerBlock(jungleLog);

  Block *mangroveLog = new LogBlock(BlockType::MANGROVE_LOG, "Mangrove Log");
  mangroveLog->setResourceId("lithos:mangrove_log");
  mangroveLog->setTexture("wood/bark/mangrove_log");
  mangroveLog->setTexture(4, "wood/treetrunk/mangrove_log_top");
  mangroveLog->setTexture(5, "wood/treetrunk/mangrove_log_top");
  registerBlock(mangroveLog);

  Block *paleOakLog = new LogBlock(BlockType::PALE_OAK_LOG, "Pale Oak Log");
  paleOakLog->setResourceId("lithos:pale_oak_log");
  paleOakLog->setTexture("wood/bark/pale_oak_log");
  paleOakLog->setTexture(4, "wood/treetrunk/pale_oak_log_top");
  paleOakLog->setTexture(5, "wood/treetrunk/pale_oak_log_top");
  registerBlock(paleOakLog);

  // New Leaves
  Block *acaciaLeaves =
      new PlantBlock(BlockType::ACACIA_LEAVES, "Acacia Leaves");
  acaciaLeaves->setResourceId("lithos:acacia_leaves");
  acaciaLeaves->setTexture("plant/leaves/acacia_leaves");
  registerBlock(acaciaLeaves);

  Block *birchLeaves = new PlantBlock(BlockType::BIRCH_LEAVES, "Birch Leaves");
  birchLeaves->setResourceId("lithos:birch_leaves");
  birchLeaves->setTexture("plant/leaves/birch_leaves");
  registerBlock(birchLeaves);

  Block *darkOakLeaves =
      new PlantBlock(BlockType::DARK_OAK_LEAVES, "Dark Oak Leaves");
  darkOakLeaves->setResourceId("lithos:dark_oak_leaves");
  darkOakLeaves->setTexture("plant/leaves/dark_oak_leaves");
  registerBlock(darkOakLeaves);

  Block *jungleLeaves =
      new PlantBlock(BlockType::JUNGLE_LEAVES, "Jungle Leaves");
  jungleLeaves->setResourceId("lithos:jungle_leaves");
  jungleLeaves->setTexture("plant/leaves/jungle_leaves");
  registerBlock(jungleLeaves);

  Block *mud = new SolidBlock(BlockType::MUD, "Mud");
  mud->setResourceId("lithos:mud");
  mud->setTexture("soil/mud");
  registerBlock(mud);

  Block *podzol = new SolidBlock(BlockType::PODZOL, "Podzol");
  podzol->setResourceId("lithos:podzol");
  podzol->setTexture("soil/podzol_side");
  podzol->setTexture(4, "soil/podzol_top");
  podzol->setTexture(5, "soil/dirt");
  registerBlock(podzol);

  Block *sandstone = new SolidBlock(BlockType::SANDSTONE, "Sandstone");
  sandstone->setResourceId("lithos:sandstone");
  sandstone->setTexture("stone/sand/sandstone"); // Side
  sandstone->setTexture(4, "stone/sand/sandstone_top");
  sandstone->setTexture(5, "stone/sand/sandstone_bottom");
  registerBlock(sandstone);

  Block *tuff = new SolidBlock(BlockType::TUFF, "Tuff");
  tuff->setResourceId("lithos:tuff");
  tuff->setTexture("stone/rock/tuff");
  registerBlock(tuff);

  Block *anthracite = new SolidBlock(BlockType::ANTHRACITE, "Anthracite");
  anthracite->setResourceId("lithos:anthracite");
  anthracite->setTexture("stone/rock/anthracite");
  registerBlock(anthracite);

  Block *bauxite = new SolidBlock(BlockType::BAUXITE, "Bauxite");
  bauxite->setResourceId("lithos:bauxite");
  bauxite->setTexture("stone/rock/bauxite");
  registerBlock(bauxite);

  Block *chalk = new SolidBlock(BlockType::CHALK, "Chalk");
  chalk->setResourceId("lithos:chalk");
  chalk->setTexture("stone/rock/chalk");
  registerBlock(chalk);

  Block *chert = new SolidBlock(BlockType::CHERT, "Chert");
  chert->setResourceId("lithos:chert");
  chert->setTexture("stone/rock/chert");
  registerBlock(chert);

  Block *clay = new SolidBlock(BlockType::CLAY, "Clay");
  clay->setResourceId("lithos:clay");
  clay->setTexture("soil/clay");
  registerBlock(clay);

  Block *claystone = new SolidBlock(BlockType::CLAYSTONE, "Claystone");
  claystone->setResourceId("lithos:claystone");
  claystone->setTexture("stone/rock/claystone");
  registerBlock(claystone);

  Block *conglomerate = new SolidBlock(BlockType::CONGLOMERATE, "Conglomerate");
  conglomerate->setResourceId("lithos:conglomerate");
  conglomerate->setTexture("stone/rock/conglomerate");
  registerBlock(conglomerate);

  Block *greenMarble = new SolidBlock(BlockType::GREEN_MARBLE, "Green Marble");
  greenMarble->setResourceId("lithos:green_marble");
  greenMarble->setTexture("stone/rock/green_marble");
  registerBlock(greenMarble);

  Block *halite = new SolidBlock(BlockType::HALITE, "Halite");
  halite->setResourceId("lithos:halite");
  halite->setTexture("stone/rock/halite");
  registerBlock(halite);

  Block *kimberlite = new SolidBlock(BlockType::KIMBERLITE, "Kimberlite");
  kimberlite->setResourceId("lithos:kimberlite");
  kimberlite->setTexture("stone/rock/kimberlite");
  registerBlock(kimberlite);

  Block *limestone = new SolidBlock(BlockType::LIMESTONE, "Limestone");
  limestone->setResourceId("lithos:limestone");
  limestone->setTexture("stone/rock/limestone");
  registerBlock(limestone);

  Block *mantle = new SolidBlock(BlockType::MANTLE, "Mantle");
  mantle->setResourceId("lithos:mantle");
  mantle->setTexture("stone/rock/mantle");
  registerBlock(mantle);

  Block *peridotite = new SolidBlock(BlockType::PERIDOTITE, "Peridotite");
  peridotite->setResourceId("lithos:peridotite");
  peridotite->setTexture("stone/rock/peridotite");
  registerBlock(peridotite);

  Block *phyllite = new SolidBlock(BlockType::PHYLLITE, "Phyllite");
  phyllite->setResourceId("lithos:phyllite");
  phyllite->setTexture("stone/rock/phyllite");
  registerBlock(phyllite);

  Block *pinkMarble = new SolidBlock(BlockType::PINK_MARBLE, "Pink Marble");
  pinkMarble->setResourceId("lithos:pink_marble");
  pinkMarble->setTexture("stone/rock/pink_marble");
  registerBlock(pinkMarble);

  Block *scoria = new SolidBlock(BlockType::SCORIA, "Scoria");
  scoria->setResourceId("lithos:scoria");
  scoria->setTexture("stone/rock/scoria");
  registerBlock(scoria);

  Block *shale = new SolidBlock(BlockType::SHALE, "Shale");
  shale->setResourceId("lithos:shale");
  shale->setTexture("stone/rock/shale");
  registerBlock(shale);

  Block *slate = new SolidBlock(BlockType::SLATE, "Slate");
  slate->setResourceId("lithos:slate");
  slate->setTexture("stone/rock/slate");
  registerBlock(slate);

  Block *suevite = new SolidBlock(BlockType::SUEVITE, "Suevite");
  suevite->setResourceId("lithos:suevite");
  suevite->setTexture("stone/rock/suevite");
  registerBlock(suevite);

  Block *whiteMarble = new SolidBlock(BlockType::WHITE_MARBLE, "White Marble");
  whiteMarble->setResourceId("lithos:white_marble");
  whiteMarble->setTexture("stone/rock/white_marble");
  registerBlock(whiteMarble);

  Block *schist = new SolidBlock(BlockType::SCHIST, "Schist");
  schist->setResourceId("lithos:schist");
  schist->setTexture("stone/rock/schist");
  registerBlock(schist);

  Block *rhyolite = new SolidBlock(BlockType::RHYOLITE, "Rhyolite");
  rhyolite->setResourceId("lithos:rhyolite");
  rhyolite->setTexture("stone/rock/rhyolite");
  registerBlock(rhyolite);

  Block *goldOre = new SolidBlock(BlockType::GOLD_ORE, "Gold Ore");
  goldOre->setResourceId("lithos:gold_ore");
  goldOre->setTexture("stone/ore/gold_ore");
  registerBlock(goldOre);

  Block *gneiss = new SolidBlock(BlockType::GNEISS, "Gneiss");
  gneiss->setResourceId("lithos:gneiss");
  gneiss->setTexture("stone/rock/gneiss");
  registerBlock(gneiss);

  Block *coarseDirt = new SolidBlock(BlockType::COARSE_DIRT, "Coarse Dirt");
  coarseDirt->setResourceId("lithos:coarse_dirt");
  coarseDirt->setTexture("soil/coarse_dirt");
  registerBlock(coarseDirt);

  Block *terraPreta = new SolidBlock(BlockType::TERRA_PRETA, "Terra Preta");
  terraPreta->setResourceId("lithos:terra_preta");
  terraPreta->setTexture("soil/terra_preta");
  registerBlock(terraPreta);

  Block *peat = new SolidBlock(BlockType::PEAT, "Peat");
  peat->setResourceId("lithos:peat");
  peat->setTexture("soil/peat");
  registerBlock(peat);

  Block *snowLayer = new LayeredBlock(BlockType::SNOW_LAYER, "Snow Layer", 8);
  snowLayer->setResourceId("lithos:snow_layer");
  snowLayer->setTexture("liquid/snow");
  registerBlock(snowLayer);

  // Dynamic Model Loading
  for (auto &pair : blocks) {
    Block *block = pair.second;
    std::string resId = block->getResourceId();
    if (resId.empty())
      continue;

    size_t colon = resId.find(':');
    if (colon != std::string::npos) {
      std::string path = resId.substr(colon + 1);
      // Check for JSON
      std::filesystem::path modelPath =
          std::filesystem::path("assets/models/block") / (path + ".json");
      if (std::filesystem::exists(modelPath)) {
        LOG_RESOURCE_INFO("Loading custom model for {} -> {}", resId,
                          modelPath.string());
        block->setRenderShape(Block::RenderShape::MODEL);
        block->setModel(modelPath);
      }
    }
  }

  LOG_INFO("BlockRegistry initialized. Registered {} blocks.", blocks.size());
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

Block *BlockRegistry::getBlock(const std::string &resourceId) {
  for (const auto &pair : blocks) {
    if (pair.second->getResourceId() == resourceId) {
      return pair.second;
    }
  }
  return defaultBlock;
}

BlockRegistry::~BlockRegistry() {
  // Cleanup
}
