#include "Block.h"
#include "blocks/AirBlock.h"
#include "blocks/FallingBlock.h"
#include "blocks/LightBlock.h"
#include "blocks/LiquidBlock.h"
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
  dirt->setTexture("dirt");
  registerBlock(dirt);

  Block *grass = new SolidBlock(BlockType::GRASS, "Grass");
  grass->setResourceId("lithos:grass");
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
  stone->setResourceId("lithos:stone");
  stone->setTexture("stone");
  registerBlock(stone);

  Block *wood = new SolidBlock(BlockType::WOOD, "Oak Log");
  wood->setResourceId("lithos:oak_log");
  wood->setTexture("oak_log");
  wood->setTexture(4, "oak_log_top");
  wood->setTexture(5, "oak_log_top");
  registerBlock(wood);

  Block *leaves = new PlantBlock(BlockType::LEAVES, "Oak Leaves");
  leaves->setResourceId("lithos:oak_leaves");
  leaves->setTexture("oak_leaves");
  registerBlock(leaves);

  Block *coal = new SolidBlock(BlockType::COAL_ORE, "Coal Ore");
  coal->setResourceId("lithos:coal_ore");
  coal->setTexture("coal_ore");
  registerBlock(coal);

  Block *iron = new SolidBlock(BlockType::IRON_ORE, "Iron Ore");
  iron->setResourceId("lithos:iron_ore");
  iron->setTexture("iron_ore");
  registerBlock(iron);

  Block *glow = new LightBlock(BlockType::GLOWSTONE, "Glowstone", 15);
  glow->setResourceId("lithos:glowstone");
  glow->setTexture("glowstone");
  registerBlock(glow);

  Block *water = new LiquidBlock(BlockType::WATER, "Water");
  water->setResourceId("lithos:water");
  water->setTexture("water_still");
  water->setTexture(0, "water_flow");
  water->setTexture(1, "water_flow");
  water->setTexture(2, "water_flow");
  water->setTexture(3, "water_flow");
  registerBlock(water);

  Block *lava = new LiquidBlock(BlockType::LAVA, "Lava");
  lava->setResourceId("lithos:lava");
  lava->setTexture("lava_still");
  lava->setTexture(0, "lava_flow");
  lava->setTexture(1, "lava_flow");
  lava->setTexture(2, "lava_flow");
  lava->setTexture(3, "lava_flow");
  registerBlock(lava);

  Block *sand = new FallingBlock(BlockType::SAND, "Sand");
  sand->setResourceId("lithos:sand");
  sand->setTexture("sand");
  registerBlock(sand);

  Block *gravel = new FallingBlock(BlockType::GRAVEL, "Gravel");
  gravel->setResourceId("lithos:gravel");
  gravel->setTexture("gravel");
  registerBlock(gravel);

  Block *snow = new SolidBlock(BlockType::SNOW, "Snow");
  snow->setResourceId("lithos:snow");
  snow->setTexture("snow");
  registerBlock(snow);

  Block *ice = new SolidBlock(BlockType::ICE, "Ice");
  ice->setResourceId("lithos:ice");
  ice->setTexture("ice");
  ice->setOpaque(false);
  registerBlock(ice);

  // Flora & Structures
  Block *cactus = new SolidBlock(BlockType::CACTUS, "Cactus");
  cactus->setResourceId("lithos:cactus");
  cactus->setTexture("cactus_side");
  cactus->setTexture(4, "cactus_top");
  cactus->setTexture(5, "cactus_bottom");
  cactus->setOpaque(false);
  registerBlock(cactus);

  Block *pine = new SolidBlock(BlockType::PINE_WOOD, "Spruce Log");
  pine->setResourceId("lithos:spruce_log");
  pine->setTexture("spruce_log");
  pine->setTexture(4, "spruce_log_top");
  pine->setTexture(5, "spruce_log_top");
  // Model loading will handle the rest if file exists
  registerBlock(pine);

  Block *pineLeaves = new PlantBlock(BlockType::PINE_LEAVES, "Spruce Leaves");
  pineLeaves->setResourceId("lithos:spruce_leaves");
  pineLeaves->setTexture("spruce_leaves");
  registerBlock(pineLeaves);

  Block *tallGrass = new PlantBlock(BlockType::TALL_GRASS, "Tall Grass");
  tallGrass->setResourceId("lithos:tall_grass");
  tallGrass->setTexture("short_grass");
  registerBlock(tallGrass);

  Block *deadBush = new PlantBlock(BlockType::DEAD_BUSH, "Dead Bush");
  deadBush->setResourceId("lithos:dead_bush");
  deadBush->setTexture("dead_bush");
  registerBlock(deadBush);

  Block *rose = new PlantBlock(BlockType::ROSE, "Rose");
  rose->setResourceId("lithos:rose");
  rose->setTexture("poppy");
  registerBlock(rose);

  Block *dryShort =
      new PlantBlock(BlockType::DRY_SHORT_GRASS, "Dry Short Grass");
  dryShort->setResourceId("lithos:dry_short_grass");
  dryShort->setTexture("short_dry_grass");
  registerBlock(dryShort);

  Block *dryTall = new PlantBlock(BlockType::DRY_TALL_GRASS, "Dry Tall Grass");
  dryTall->setResourceId("lithos:dry_tall_grass");
  dryTall->setTexture("tall_dry_grass");
  registerBlock(dryTall);

  // New blocks
  Block *obsidian = new SolidBlock(BlockType::OBSIDIAN, "Obsidian");
  obsidian->setResourceId("lithos:obsidian");
  obsidian->setTexture("obsidian");
  registerBlock(obsidian);

  Block *cobblestone = new SolidBlock(BlockType::COBBLESTONE, "Cobblestone");
  cobblestone->setResourceId("lithos:cobblestone");
  cobblestone->setTexture("cobblestone");
  registerBlock(cobblestone);

  // Wood planks with metadata support: 0 = oak, 1 = spruce
  MetadataBlock *woodPlanks =
      new MetadataBlock(BlockType::WOOD_PLANKS, "Wood Planks");
  woodPlanks->setResourceId("lithos:planks"); // Or separate
  woodPlanks->setTextureForMetadata(0, "oak_planks");
  woodPlanks->setTextureForMetadata(1, "spruce_planks");
  registerBlock((Block *)woodPlanks);

  // Custom Mesh Blocks
  Block *stoneSlab = new SlabBlock(BlockType::STONE_SLAB, "Stone Slab");
  stoneSlab->setResourceId("lithos:stone_slab");
  stoneSlab->setTexture("stone");
  registerBlock(stoneSlab);

  Block *woodStairs = new StairBlock(BlockType::WOOD_STAIRS, "Oak Stairs");
  woodStairs->setResourceId("lithos:oak_stairs");
  woodStairs->setTexture("oak_planks");
  registerBlock(woodStairs);

  // Geological Blocks
  Block *andesite = new SolidBlock(BlockType::ANDESITE, "Andesite");
  andesite->setResourceId("lithos:andesite");
  andesite->setTexture("andesite");
  registerBlock(andesite);

  Block *basalt = new SolidBlock(BlockType::BASALT, "Basalt");
  basalt->setResourceId("lithos:basalt");
  basalt->setTexture("basalt_side");
  basalt->setTexture(4, "basalt_top");
  basalt->setTexture(5, "basalt_top");
  registerBlock(basalt);

  Block *diorite = new SolidBlock(BlockType::DIORITE, "Diorite");
  diorite->setResourceId("lithos:diorite");
  diorite->setTexture("diorite");
  registerBlock(diorite);

  Block *granite = new SolidBlock(BlockType::GRANITE, "Granite");
  granite->setResourceId("lithos:granite");
  granite->setTexture("granite");
  registerBlock(granite);

  Block *mud = new SolidBlock(BlockType::MUD, "Mud");
  mud->setResourceId("lithos:mud");
  mud->setTexture("mud");
  registerBlock(mud);

  Block *podzol = new SolidBlock(BlockType::PODZOL, "Podzol");
  podzol->setResourceId("lithos:podzol");
  podzol->setTexture("podzol_side");
  podzol->setTexture(4, "podzol_top");
  podzol->setTexture(5, "dirt");
  registerBlock(podzol);

  Block *sandstone = new SolidBlock(BlockType::SANDSTONE, "Sandstone");
  sandstone->setResourceId("lithos:sandstone");
  sandstone->setTexture("sandstone"); // Side
  sandstone->setTexture(4, "sandstone_top");
  sandstone->setTexture(5, "sandstone_bottom");
  registerBlock(sandstone);

  Block *tuff = new SolidBlock(BlockType::TUFF, "Tuff");
  tuff->setResourceId("lithos:tuff");
  tuff->setTexture("tuff");
  registerBlock(tuff);

  Block *anthracite = new SolidBlock(BlockType::ANTHRACITE, "Anthracite");
  anthracite->setResourceId("lithos:anthracite");
  anthracite->setTexture("anthracite");
  registerBlock(anthracite);

  Block *bauxite = new SolidBlock(BlockType::BAUXITE, "Bauxite");
  bauxite->setResourceId("lithos:bauxite");
  bauxite->setTexture("bauxite");
  registerBlock(bauxite);

  Block *chalk = new SolidBlock(BlockType::CHALK, "Chalk");
  chalk->setResourceId("lithos:chalk");
  chalk->setTexture("chalk");
  registerBlock(chalk);

  Block *chert = new SolidBlock(BlockType::CHERT, "Chert");
  chert->setResourceId("lithos:chert");
  chert->setTexture("chert");
  registerBlock(chert);

  Block *clay = new SolidBlock(BlockType::CLAY, "Clay");
  clay->setResourceId("lithos:clay");
  clay->setTexture("clay");
  registerBlock(clay);

  Block *claystone = new SolidBlock(BlockType::CLAYSTONE, "Claystone");
  claystone->setResourceId("lithos:claystone");
  claystone->setTexture("claystone");
  registerBlock(claystone);

  Block *conglomerate = new SolidBlock(BlockType::CONGLOMERATE, "Conglomerate");
  conglomerate->setResourceId("lithos:conglomerate");
  conglomerate->setTexture("conglomerate");
  registerBlock(conglomerate);

  Block *greenMarble = new SolidBlock(BlockType::GREEN_MARBLE, "Green Marble");
  greenMarble->setResourceId("lithos:green_marble");
  greenMarble->setTexture("green_marble");
  registerBlock(greenMarble);

  Block *halite = new SolidBlock(BlockType::HALITE, "Halite");
  halite->setResourceId("lithos:halite");
  halite->setTexture("halite");
  registerBlock(halite);

  Block *kimberlite = new SolidBlock(BlockType::KIMBERLITE, "Kimberlite");
  kimberlite->setResourceId("lithos:kimberlite");
  kimberlite->setTexture("kimberlite");
  registerBlock(kimberlite);

  Block *limestone = new SolidBlock(BlockType::LIMESTONE, "Limestone");
  limestone->setResourceId("lithos:limestone");
  limestone->setTexture("limestone");
  registerBlock(limestone);

  Block *mantle = new SolidBlock(BlockType::MANTLE, "Mantle");
  mantle->setResourceId("lithos:mantle");
  mantle->setTexture("mantle");
  registerBlock(mantle);

  Block *peridotite = new SolidBlock(BlockType::PERIDOTITE, "Peridotite");
  peridotite->setResourceId("lithos:peridotite");
  peridotite->setTexture("peridotite");
  registerBlock(peridotite);

  Block *phyllite = new SolidBlock(BlockType::PHYLLITE, "Phyllite");
  phyllite->setResourceId("lithos:phyllite");
  phyllite->setTexture("phyllite");
  registerBlock(phyllite);

  Block *pinkMarble = new SolidBlock(BlockType::PINK_MARBLE, "Pink Marble");
  pinkMarble->setResourceId("lithos:pink_marble");
  pinkMarble->setTexture("pink_marble");
  registerBlock(pinkMarble);

  Block *scoria = new SolidBlock(BlockType::SCORIA, "Scoria");
  scoria->setResourceId("lithos:scoria");
  scoria->setTexture("scoria");
  registerBlock(scoria);

  Block *shale = new SolidBlock(BlockType::SHALE, "Shale");
  shale->setResourceId("lithos:shale");
  shale->setTexture("shale");
  registerBlock(shale);

  Block *slate = new SolidBlock(BlockType::SLATE, "Slate");
  slate->setResourceId("lithos:slate");
  slate->setTexture("slate");
  registerBlock(slate);

  Block *suevite = new SolidBlock(BlockType::SUEVITE, "Suevite");
  suevite->setResourceId("lithos:suevite");
  suevite->setTexture("suevite");
  registerBlock(suevite);

  Block *whiteMarble = new SolidBlock(BlockType::WHITE_MARBLE, "White Marble");
  whiteMarble->setResourceId("lithos:white_marble");
  whiteMarble->setTexture("white_marble");
  registerBlock(whiteMarble);

  Block *schist = new SolidBlock(BlockType::SCHIST, "Schist");
  schist->setResourceId("lithos:schist");
  schist->setTexture("schist");
  registerBlock(schist);

  Block *rhyolite = new SolidBlock(BlockType::RHYOLITE, "Rhyolite");
  rhyolite->setResourceId("lithos:rhyolite");
  rhyolite->setTexture("rhyolite");
  registerBlock(rhyolite);

  Block *goldOre = new SolidBlock(BlockType::GOLD_ORE, "Gold Ore");
  goldOre->setResourceId("lithos:gold_ore");
  goldOre->setTexture("gold_ore");
  registerBlock(goldOre);

  Block *gneiss = new SolidBlock(BlockType::GNEISS, "Gneiss");
  gneiss->setResourceId("lithos:gneiss");
  gneiss->setTexture("gneiss");
  registerBlock(gneiss);

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

BlockRegistry::~BlockRegistry() {
  // Cleanup
}
