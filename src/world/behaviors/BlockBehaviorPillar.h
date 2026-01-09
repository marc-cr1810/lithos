#ifndef BLOCK_BEHAVIOR_PILLAR_H
#define BLOCK_BEHAVIOR_PILLAR_H

#include "BlockBehavior.h"

class BlockBehaviorPillar : public BlockBehavior {
public:
  BlockBehaviorPillar(Block *block);

  block_id getPlacedBlockID(World &world, int x, int y, int z,
                            const glm::vec3 &playerPos,
                            const glm::vec3 &playerHeading, int clickedFace,
                            block_id currentId) const override;
};

#endif
