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
  if (dist(rng) > growthChance) {
    return;
  }

  if (!nextBlock.empty()) {
    // Note: getBlockId usage requires looking up by domain:name if it has it
    // Assuming nextBlock is fully qualified or we might need helper
    block_id nextID = BlockRegistry::getInstance().getBlockId(nextBlock);
    if (nextID != 0) {
      world.setBlock(x, y, z, nextID);
    }
  }
}
