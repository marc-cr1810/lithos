#ifndef BLOCK_BEHAVIOR_HORIZONTAL_ORIENTABLE_H
#define BLOCK_BEHAVIOR_HORIZONTAL_ORIENTABLE_H

#include "BlockBehavior.h"
#include <cmath>

class BlockBehaviorHorizontalOrientable : public BlockBehavior {
public:
  using BlockBehavior::BlockBehavior;

  void onLoaded(const nlohmann::json &properties) override {
    if (properties.contains("dropBlockFace")) {
      dropBlockFace = properties["dropBlockFace"];
    }
    // TODO: Support custom variant group code logic from JSON
  }

  block_id getPlacedBlockID(World &world, int x, int y, int z,
                            const glm::vec3 &playerPos,
                            const glm::vec3 &playerHeading, int clickedFace,
                            block_id currentId) const override {
    // 1. Determine Facing
    // Heading is direction vector. Convert to cardinal direction (N, E, S, W)
    // Lithos coordinates: X=East, Z=South (Right-handed Y-up usually)
    // Let's assume standard:
    // N: -Z
    // S: +Z
    // E: +X
    // W: -X

    // Simplistic quadrant check
    // Heading vector (x, z):
    // max(|x|, |z|) determines axis.
    float absX = std::abs(playerHeading.x);
    float absZ = std::abs(playerHeading.z);

    std::string facing = "north";

    if (absX > absZ) {
      if (playerHeading.x > 0)
        facing = "west"; // Facing +X? Wait.
      // If player looks +X (East), they place a block facing West (depends on
      // block front?) Usually "HorizontalOrientable" rotates the front face
      // towards player. So if player looks East (+X), block front should point
      // West (-X).
      else
        facing = "east";
    } else {
      if (playerHeading.z > 0)
        facing = "north"; // Facing +Z (South) -> Block faces North
      else
        facing = "south";
    }

    // VS Logic: GetSuggestedHVOrientation
    // It actually returns the face the player is LOOKING AT.

    // Let's stick to standard behavior: Block Front faces Player.
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

    // 2. Construct new Texture/Block Name
    // We assume the BlockLoader uses underscores for variants (e.g.
    // block_north).

    std::string resId = block->getResourceId();
    // Strip existing direction from resId
    // We check for _north, _east, _south, _west at the end
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

    // Check if valid
    Block *newBlock = BlockRegistry::getInstance().getBlock(newResId);
    if (newBlock && newBlock != BlockRegistry::getInstance().getBlock(0)) {
      return newBlock->getId();
    }

    return currentId;
  }

private:
  std::string dropBlockFace = "north";
};

#endif
