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
  blocks[defaultBlock->getId()] = defaultBlock;

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
        LOG_RESOURCE_TRACE("Loading custom model for {} -> {}", resId,
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

void BlockRegistry::addBlockToTab(const std::string &tabCode, Block *block) {
  for (auto &tab : creativeTabs) {
    if (tab.code == tabCode) {
      tab.blocks.push_back(block);
      block->addCreativeTab(tabCode);
      return;
    }
  }
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
