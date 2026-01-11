#include "Block.h"
#include "BlockLoader.h"
#include "blocks/AirBlock.h"
#include "blocks/FallingBlock.h"
#include "blocks/LayeredBlock.h"
// #include "blocks/LightBlock.h"
#include "blocks/LiquidBlock.h"
#include "blocks/LogBlock.h"
// #include "blocks/MetadataBlock.h"
#include "blocks/PlantBlock.h"
#include "blocks/SlabBlock.h"
#include "blocks/SolidBlock.h"
#include "blocks/StairBlock.h"
#include "debug/Logger.h"
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <nlohmann/json.hpp>

BlockRegistry &BlockRegistry::getInstance() {
  static BlockRegistry instance;
  return instance;
}

BlockRegistry::BlockRegistry() {
  // Default to Air to avoid crashes
  defaultBlock = new AirBlock();
  defaultBlock->setId(AIR);
  blocks[AIR] = defaultBlock;
  blocksByResourceId["lithos:air"] = defaultBlock;
  nextId = 1; // Air is 0, next is 1

  // Load Creative Tabs
  std::ifstream f("assets/config/creative_tabs.json");
  if (f.is_open()) {
    nlohmann::json j;
    f >> j;
    if (j.contains("tabConfigs")) {
      for (const auto &tc : j.at("tabConfigs")) {
        CreativeTab tab;
        tab.code = tc.at("code").get<std::string>();
        tab.listOrder = tc.at("listOrder").get<int>();
        creativeTabs.push_back(tab);
      }
      // Sort by listOrder
      std::sort(creativeTabs.begin(), creativeTabs.end(),
                [](const CreativeTab &a, const CreativeTab &b) {
                  return a.listOrder < b.listOrder;
                });
    }
  } else {
    LOG_WARN(
        "Creative tabs config not found: assets/config/creative_tabs.json");
  }

  // Load JSON block definitions first
  LOG_INFO("Loading JSON block definitions...");
  std::filesystem::path blockTypesPath = "assets/blocktypes";
  if (std::filesystem::exists(blockTypesPath)) {
    auto jsonBlocks = BlockLoader::loadFromDirectory(blockTypesPath);
    for (Block *block : jsonBlocks) {
      registerBlock(block);
    }
  } else {
    LOG_WARN("Block definitions directory not found: {}",
             blockTypesPath.string());
  }

  registerBlock(new AirBlock()); // Air doesn't strictly need ID if it's
                                 // default? Or "lithos:air"

  // NOW register blocks to creative tabs (after all blocks are loaded)
  // This must be done AFTER block loading to avoid static initialization
  // deadlock
  LOG_INFO("Registering blocks to creative tabs...");
  for (auto &pair : blocks) {
    Block *block = pair.second;
    const auto &creativeTabsList = block->getCreativeTabs();

    for (const std::string &tabCode : creativeTabsList) {
      // Find the tab and add the block
      for (auto &tab : creativeTabs) {
        if (tab.code == tabCode) {
          tab.blocks.push_back(block);
          break;
        }
      }
    }
  }
  LOG_INFO("Creative tab registration complete");

  LOG_INFO("BlockRegistry initialized. Registered {} blocks.", blocks.size());
}

void BlockRegistry::registerBlock(Block *block) {
  // If block doesn't have an ID yet (is 0 but not air), assign it
  if (block->getResourceId() != "lithos:air" && block->getId() == AIR) {
    block->setId(nextId++);
    LOG_DEBUG("Registered block: {} with dynamic ID {}", block->getResourceId(),
              block->getId());
  }

  blocks[block->getId()] = block;
  if (block->getId() >= blockVector.size()) {
    blockVector.resize(block->getId() + 1, defaultBlock);
  }
  blockVector[block->getId()] = block;

  if (!block->getResourceId().empty()) {
    blocksByResourceId[block->getResourceId()] = block;
  }
}

void BlockRegistry::addBlockToTab(const std::string &tabCode, Block *block) {
  for (auto &tab : creativeTabs) {
    if (tab.code == tabCode) {
      tab.blocks.push_back(block);
      block->addCreativeTab(tabCode);
      return;
    }
  }
}

Block *BlockRegistry::getBlock(const std::string &resourceId) {
  auto it = blocksByResourceId.find(resourceId);
  if (it != blocksByResourceId.end()) {
    return it->second;
  }

  // Namespace Fallback Logic
  // 1. If no namespace, try adding 'lithos:'
  if (resourceId.find(':') == std::string::npos) {
    auto it2 = blocksByResourceId.find("lithos:" + resourceId);
    if (it2 != blocksByResourceId.end())
      return it2->second;
  }
  // 2. If 'lithos:' namespace, try removing it
  else if (resourceId.size() >= 7 && resourceId.compare(0, 7, "lithos:") == 0) {
    auto it3 = blocksByResourceId.find(resourceId.substr(7));
    if (it3 != blocksByResourceId.end())
      return it3->second;
  }

  return defaultBlock;
}

block_id BlockRegistry::getBlockId(const std::string &resourceId) {
  return getBlock(resourceId)->getId();
}

BlockRegistry::~BlockRegistry() {
  // Cleanup
}
