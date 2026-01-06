#include "BlockFactory.h"
#include "../debug/Logger.h"

#include "blocks/FallingBlock.h"
#include "blocks/GrassBlock.h"
#include "blocks/LavaBlock.h"
#include "blocks/LayeredBlock.h"
#include "blocks/LeavesBlock.h"
#include "blocks/LiquidBlock.h"
#include "blocks/LogBlock.h"
#include "blocks/PlantBlock.h"
#include "blocks/SlabBlock.h"
#include "blocks/SolidBlock.h"
#include "blocks/StairBlock.h"
#include "blocks/WaterBlock.h"

BlockFactory &BlockFactory::getInstance() {
  static BlockFactory instance;
  return instance;
}

BlockFactory::BlockFactory() {
  // Register Core Blocks
  registerBlock("SolidBlock", [](uint8_t id, const std::string &name) {
    return new SolidBlock(id, name);
  });
  registerBlock("LiquidBlock", [](uint8_t id, const std::string &name) {
    return new LiquidBlock(id, name);
  });
  registerBlock("PlantBlock", [](uint8_t id, const std::string &name) {
    return new PlantBlock(id, name);
  });
  registerBlock("FallingBlock", [](uint8_t id, const std::string &name) {
    return new FallingBlock(id, name);
  });
  registerBlock("LogBlock", [](uint8_t id, const std::string &name) {
    return new LogBlock(id, name);
  });
  registerBlock("LayeredBlock", [](uint8_t id, const std::string &name) {
    return new LayeredBlock(id, name);
  });
  registerBlock("SlabBlock", [](uint8_t id, const std::string &name) {
    return new SlabBlock(id, name);
  });
  registerBlock("StairBlock", [](uint8_t id, const std::string &name) {
    return new StairBlock(id, name);
  });

  // Specialized Blocks
  registerBlock("GrassBlock", [](uint8_t id, const std::string &name) {
    return new GrassBlock(id, name);
  });
  registerBlock("WaterBlock", [](uint8_t id, const std::string &name) {
    return new WaterBlock(id, name);
  });
  registerBlock("LavaBlock", [](uint8_t id, const std::string &name) {
    return new LavaBlock(id, name);
  });
  registerBlock("LeavesBlock", [](uint8_t id, const std::string &name) {
    return new LeavesBlock(id, name);
  });
}

void BlockFactory::registerBlock(const std::string &className,
                                 BlockConstructor constructor) {
  constructors[className] = constructor;
}

Block *BlockFactory::createBlock(const std::string &className, uint8_t id,
                                 const std::string &variantCode) {
  auto it = constructors.find(className);
  if (it != constructors.end()) {
    return it->second(id, variantCode);
  }
  LOG_ERROR("BlockFactory: Unknown block class '{}' for block '{}'. Defaulting "
            "to SolidBlock.",
            className, variantCode);
  return new SolidBlock(id, variantCode);
}
