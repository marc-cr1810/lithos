#ifndef BLOCK_BEHAVIOR_HORIZONTAL_ORIENTABLE_H
#define BLOCK_BEHAVIOR_HORIZONTAL_ORIENTABLE_H

#include "BlockBehavior.h"
#include <string>

class BlockBehaviorHorizontalOrientable : public BlockBehavior {
public:
  BlockBehaviorHorizontalOrientable(Block *block);

  void onLoaded(const nlohmann::json &properties) override;
  block_id getPlacedBlockID(World &world, int x, int y, int z,
                            const glm::vec3 &playerPos,
                            const glm::vec3 &playerHeading, int clickedFace,
                            block_id currentId) const override;

private:
  std::string dropBlockFace = "north";
};

#endif
