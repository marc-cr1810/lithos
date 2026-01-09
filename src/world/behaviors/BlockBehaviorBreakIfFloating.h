#ifndef BLOCK_BEHAVIOR_BREAK_IF_FLOATING_H
#define BLOCK_BEHAVIOR_BREAK_IF_FLOATING_H

#include "BlockBehavior.h"

class BlockBehaviorBreakIfFloating : public BlockBehavior {
public:
  using BlockBehavior::BlockBehavior;

  void onNeighborChange(World &world, int x, int y, int z, int nx, int ny,
                        int nz) const override;
};

#endif
