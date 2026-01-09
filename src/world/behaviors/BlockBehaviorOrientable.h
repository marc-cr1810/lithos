#ifndef BLOCK_BEHAVIOR_ORIENTABLE_H
#define BLOCK_BEHAVIOR_ORIENTABLE_H

#include "BlockBehavior.h"
#include <cmath>
#include <string>
#include <vector>

class BlockBehaviorOrientable : public BlockBehavior {
public:
  using BlockBehavior::BlockBehavior;

  void onLoaded(const nlohmann::json &properties) override {
    // Potentially load thresholds or specific variant codes
  }

  block_id getPlacedBlockID(World &world, int x, int y, int z,
                            const glm::vec3 &playerPos,
                            const glm::vec3 &playerHeading, int clickedFace,
                            block_id currentId) const override {

    std::string facing = "north";

    // 1. Check Vertical Facing (Pitch)
    // playerHeading.y is sin(pitch). Range -1 to 1.
    // If > 0.5 (approx 30 deg), looking up.
    // If < -0.5, looking down.

    // Standard "Face Player" logic:
    // Look Down (y < -0.5) -> Block faces UP (towards player)
    // Look Up (y > 0.5) -> Block faces DOWN (towards player)

    float vertThreshold = 0.5f; // Configurable?

    if (playerHeading.y < -vertThreshold) {
      facing = "up";
    } else if (playerHeading.y > vertThreshold) {
      facing = "down";
    } else {
      // Horizontal Logic (same as HorizontalOrientable)
      float absX = std::abs(playerHeading.x);
      float absZ = std::abs(playerHeading.z);

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
    }

    // 2. Construct new ID
    std::string resId = block->getResourceId();

    // Strip existing direction
    const std::vector<std::string> resDirs = {"_north", "_east", "_south",
                                              "_west",  "_up",   "_down"};
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
};

#endif
