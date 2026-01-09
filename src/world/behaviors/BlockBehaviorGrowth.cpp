#include "BlockBehaviorGrowth.h"
#include "../Block.h"
#include "../World.h"
#include "BlockBehaviorRegistrar.h"

static BlockBehaviorRegistrar registrar("Growth", [](Block *block) {
  return std::make_shared<BlockBehaviorGrowth>(block);
});
// Register lowercase alias too
static BlockBehaviorRegistrar registrarLower("growth", [](Block *block) {
  return std::make_shared<BlockBehaviorGrowth>(block);
});

BlockBehaviorGrowth::BlockBehaviorGrowth(Block *block) : BlockBehavior(block) {
  block->setRandomTickable(true);
  LOG_INFO("BlockBehaviorGrowth attached to block, randomTickable set to true");
}

void BlockBehaviorGrowth::onLoaded(const nlohmann::json &properties) {
  if (properties.contains("maxStage")) {
    maxStage = properties["maxStage"].get<int>();
  }
  if (properties.contains("growthChance")) {
    growthChance = properties["growthChance"].get<float>();
  }
  if (properties.contains("nextBlock")) {
    nextBlock = properties["nextBlock"].get<std::string>();
  }
}

void BlockBehaviorGrowth::onRandomTick(World &world, int x, int y, int z,
                                       std::mt19937 &rng) const {
  std::uniform_real_distribution<float> dist(0.0f, 1.0f);
  float roll = dist(rng);

  // LOG_INFO("Growth tick at ({},{},{}) roll={:.2f} chance={:.2f}", x, y, z,
  // roll, growthChance);

  if (roll > growthChance) {
    return;
  }

  if (!nextBlock.empty()) {
    block_id nextID = BlockRegistry::getInstance().getBlockId(nextBlock);

    if (nextID != 0) {
      world.queueBlockChange(x, y, z, nextID);
    } else {
      LOG_ERROR("Growth failed - could not resolve nextBlock '{}'", nextBlock);
    }
  } else {
    LOG_WARN("Growth behavior has empty nextBlock");
  }
}
