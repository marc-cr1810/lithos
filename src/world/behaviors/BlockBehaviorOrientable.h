#ifndef BLOCK_BEHAVIOR_ORIENTABLE_H
#define BLOCK_BEHAVIOR_ORIENTABLE_H

#include "BlockBehavior.h"

class BlockBehaviorOrientable : public BlockBehavior {
public:
  BlockBehaviorOrientable(Block *block);

  void onLoaded(const nlohmann::json &properties) override;

  block_id getPlacedBlockID(World &world, int x, int y, int z,
                            const glm::vec3 &playerPos,
                            const glm::vec3 &playerHeading, int clickedFace,
                            block_id currentId) const override;
};

#endif
