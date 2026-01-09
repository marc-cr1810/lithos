#include "BlockBehaviorHorizontalOrientable.h"
#include "../Block.h"
#include "BlockBehaviorRegistrar.h"
#include <cmath>

static BlockBehaviorRegistrar
    registrar("HorizontalOrientable", [](Block *block) {
      return std::make_shared<BlockBehaviorHorizontalOrientable>(block);
    });

BlockBehaviorHorizontalOrientable::BlockBehaviorHorizontalOrientable(
    Block *block)
    : BlockBehavior(block) {}

void BlockBehaviorHorizontalOrientable::onLoaded(
    const nlohmann::json &properties) {
  if (properties.contains("dropBlockFace")) {
    dropBlockFace = properties["dropBlockFace"];
  }
}

block_id BlockBehaviorHorizontalOrientable::getPlacedBlockID(
    World &world, int x, int y, int z, const glm::vec3 &playerPos,
    const glm::vec3 &playerHeading, int clickedFace, block_id currentId) const {

  float absX = std::abs(playerHeading.x);
  float absZ = std::abs(playerHeading.z);

  std::string facing = "north";

  if (absX > absZ) {
    if (playerHeading.x > 0)
      facing = "west";
    else
      facing = "east";
  } else {
    if (playerHeading.z > 0)
      facing = "north";
    else
      facing = "south";
  }

  std::string resId = block->getResourceId();
  // Strip existing direction from resId
  const std::vector<std::string> resDirs = {"_north", "_east", "_south",
                                            "_west"};
  for (const auto &d : resDirs) {
    if (resId.length() >= d.length() &&
        resId.compare(resId.length() - d.length(), d.length(), d) == 0) {
      resId = resId.substr(0, resId.length() - d.length());
      break;
    }
  }

  std::string newResId = resId + "_" + facing;

  Block *newBlock = BlockRegistry::getInstance().getBlock(newResId);
  if (newBlock && newBlock != BlockRegistry::getInstance().getBlock(0)) {
    return newBlock->getId();
  }

  return currentId;
}
