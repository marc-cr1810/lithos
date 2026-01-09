#pragma once

#include "BlockBehavior.h"
#include <string>
#include <vector>

class BlockBehaviorPillar : public BlockBehavior {
public:
  BlockBehaviorPillar(Block *block) : BlockBehavior(block) {}

  block_id getPlacedBlockID(World &world, int x, int y, int z,
                            const glm::vec3 &playerPos,
                            const glm::vec3 &playerHeading, int clickedFace,
                            block_id currentId) const override {

    // Use member 'block' from base class
    std::string baseName = block->getResourceId();

    // Determine the axis state based on the clicked face
    std::string state = "ud"; // Default to Up/Down

    // Face mapping:
    // 0: Front (Z+) -> South
    // 1: Back (Z-) -> North
    // 2: Left (X-) -> West
    // 3: Right (X+) -> East
    // 4: Top (Y+) -> Up
    // 5: Bottom (Y-) -> Down

    if (clickedFace == 4 || clickedFace == 5) {
      state = "ud";
    } else if (clickedFace == 0 || clickedFace == 1) {
      state = "ns";
    } else if (clickedFace == 2 || clickedFace == 3) {
      state = "we";
    }

    // Construct the new variant name
    // Strip existing suffix and append the new one.
    // Known suffixes: "_ud", "_ns", "_we".
    std::string newName = baseName;

    if (newName.length() > 3) {
      std::string suffix = newName.substr(newName.length() - 3);
      if (suffix == "_ud" || suffix == "_ns" || suffix == "_we") {
        newName = newName.substr(0, newName.length() - 3);
      }
    }

    newName += "_" + state;

    block_id res = BlockRegistry::getInstance().getBlockId(newName);
    if (res == 0) {
      LOG_ERROR("Pillar: Generated '{}' from '{}' (state '{}') but ID is 0!",
                newName, baseName, state);
    }

    return res;
  }
};
