#include "BlockBehaviorFiniteSpreadingLiquid.h"
#include "../Block.h"
#include "../World.h"
#include "BlockBehaviorRegistrar.h"
#include <algorithm>

// Register the behavior
static BlockBehaviorRegistrar
    registrar("FiniteSpreadingLiquid", [](Block *block) {
      return std::make_shared<BlockBehaviorFiniteSpreadingLiquid>(block);
    });

BlockBehaviorFiniteSpreadingLiquid::BlockBehaviorFiniteSpreadingLiquid(
    Block *block)
    : BlockBehavior(block) {}

void BlockBehaviorFiniteSpreadingLiquid::onLoaded(
    const nlohmann::json &properties) {
  if (properties.contains("spreadDelay")) {
    spreadDelay = properties.at("spreadDelay").get<int>();
  }
  if (properties.contains("liquidCollisionSound")) {
    liquidCollisionSound =
        properties.at("liquidCollisionSound").get<std::string>();
  }
  if (properties.contains("sourceReplacementCode")) {
    sourceReplacementCode =
        properties.at("sourceReplacementCode").get<std::string>();
  }
  if (properties.contains("flowingReplacementCode")) {
    flowingReplacementCode =
        properties.at("flowingReplacementCode").get<std::string>();
  }
  if (properties.contains("collidesWith")) {
    if (properties.at("collidesWith").is_array()) {
      for (const auto &val : properties.at("collidesWith")) {
        collidesWith.push_back(val.get<std::string>());
      }
    } else if (properties.at("collidesWith").is_string()) {
      collidesWith.push_back(properties.at("collidesWith").get<std::string>());
    }
  }
}

void BlockBehaviorFiniteSpreadingLiquid::onPlace(World &world, int x, int y,
                                                 int z) const {
  world.scheduleBlockUpdate(x, y, z,
                            spreadDelay / 50); // Convert ms to ticks (approx)
}

void BlockBehaviorFiniteSpreadingLiquid::onNeighborChange(World &world, int x,
                                                          int y, int z, int nx,
                                                          int ny,
                                                          int nz) const {
  world.scheduleBlockUpdate(x, y, z, spreadDelay / 50);
}

void BlockBehaviorFiniteSpreadingLiquid::update(World &world, int x, int y,
                                                int z) const {
  trySpread(world, x, y, z);
}

block_id BlockBehaviorFiniteSpreadingLiquid::getLiquidBlockId(
    const std::string &baseCode, const std::string &flowDir, int level) const {
  // Construct variant name: code-{flowDir}-{level}
  // Example: water-still-7, water-n-6
  std::string variantName =
      baseCode + "-" + flowDir + "-" + std::to_string(level);

  // Prefer lithos: prefix check? BlockRegistry::getBlockId adds it if missing
  // usually? Or check raw.
  block_id id =
      BlockRegistry::getInstance().getBlockId("lithos:" + variantName);
  if (id == 0) { // Try without prefix
    id = BlockRegistry::getInstance().getBlockId(variantName);
  }
  return id;
}

void BlockBehaviorFiniteSpreadingLiquid::trySpread(World &world, int x, int y,
                                                   int z) const {
  ChunkBlock current = world.getBlock(x, y, z);
  int currentLevel = current.getBlock()->getLiquidLevel();
  if (currentLevel <= 0)
    return;

  // Assume baseCode is the prefix before the last two dashes?
  // e.g. "water-still-7" -> "water"
  // e.g. "water-n-6" -> "water"
  // Simple heuristic: extract base from current block name
  // Actually, we can just use "water" or "lava" based on liquid type?
  // Better: Parse `block->getName()`.
  std::string name = block->getName();
  // Split by '-'
  std::string baseCode = "water"; // Default
  if (name.find("lava") == 0)
    baseCode = "lava";
  else if (name.find("water") == 0)
    baseCode = "water";
  else {
    // Try to deduce.
    size_t firstDash = name.find('-');
    if (firstDash != std::string::npos)
      baseCode = name.substr(0, firstDash);
  }

  // 1. Try Spread Down
  ChunkBlock below = world.getBlock(x, y - 1, z);
  bool spreadDown = false;

  // Collision Logic (Lava vs Water)
  // TODO: specialized collision

  if (below.isActive()) {
    // If below is replacable (air, grass, etc) OR is same liquid
    if (below.getBlock()->isReplaceable() ||
        (below.getBlock()->isLiquid() &&
         below.getBlock()->getLiquidLevel() < 7 &&
         below.getType() != current.getType())) {
      // If compatible liquid?
      // Determine if valid target
    }
  }

  // Simplified Logic:
  // If down is air or replaceable -> place Source-like falling liquid (level 7?
  // or same level?) VS: falling liquid usually has level 7 but reduced visual
  // (narrower). Lithos: We will spread logic "down" (variant "d").

  if (y > 0) {
    ChunkBlock downBlock = world.getBlock(x, y - 1, z);
    if (downBlock.getBlock()->isReplaceable() ||
        (downBlock.getBlock()->isLiquid() &&
         !downBlock.getBlock()->isLiquidSource() &&
         downBlock.getBlock()->getLiquidLevel() <
             7)) { // Actually VS replaces non-sources
      // Spread down as "d" (down) variant, level 7 (falling stream is full
      // strength)
      block_id downId = getLiquidBlockId(
          baseCode, "d",
          7); // Falling is usually max level logic-wise or keeps source level?
      // VS: falling water decreases level every block? No, usually falls
      // indefinitely. Let's use level 6 or 7.
      if (downId != 0 && downId != downBlock.getType()) {
        world.setBlock(x, y - 1, z, downId);
        spreadDown = true;
      }
    }
  }

  // If we spread down, we might not spread sideways as much?
  // VS logic: if spreading down, sidebar spread is reduced or stopped?
  // Let's implement sideways spread regardless for now (classic MC style).

  if (currentLevel <= 1)
    return; // Can't spread sideways if level 1

  int nextLevel = currentLevel - 1;

  // Sideways: N, E, S, W
  int dx[] = {0, 1, 0, -1};
  int dz[] = {-1, 0, 1, 0};
  std::string dirs[] = {"n", "e", "s", "w"};

  for (int i = 0; i < 4; ++i) {
    int nx = x + dx[i];
    int nz = z + dz[i];
    ChunkBlock neighbor = world.getBlock(nx, y, nz);

    if (neighbor.getBlock()->isReplaceable() ||
        (neighbor.getBlock()->isLiquid() &&
         neighbor.getBlock()->getLiquidLevel() < nextLevel)) {
      // Check if we can spread
      // Avoid overwriting same level or higher
      if (neighbor.getBlock()->isLiquid() &&
          neighbor.getBlock()->getLiquidLevel() >= nextLevel)
        continue;

      // Place Flowing Block
      block_id flowId = getLiquidBlockId(baseCode, dirs[i], nextLevel);
      if (flowId != 0 && flowId != neighbor.getType()) {
        world.setBlock(nx, y, nz, flowId);
      }
    }
  }
}
