#ifndef BLOCK_BEHAVIOR_BREAK_IF_FLOATING_H
#define BLOCK_BEHAVIOR_BREAK_IF_FLOATING_H

#include "../World.h"
#include "BlockBehavior.h"
#include "glm/glm.hpp"

class BlockBehaviorBreakIfFloating : public BlockBehavior {
public:
  using BlockBehavior::BlockBehavior;

  void onNeighborChange(World &world, int x, int y, int z, int nx, int ny,
                        int nz) const override {
    // Check all 6 sides
    bool surroundedByAir = true;

    static const int offsets[6][3] = {{-1, 0, 0}, {1, 0, 0},  {0, -1, 0},
                                      {0, 1, 0},  {0, 0, -1}, {0, 0, 1}};

    for (int i = 0; i < 6; ++i) {
      int ox = x + offsets[i][0];
      int oy = y + offsets[i][1];
      int oz = z + offsets[i][2];

      // If any neighbor is solid, we are stable
      if (world.getBlock(ox, oy, oz).isSolid()) {
        surroundedByAir = false;
        break;
      }
    }

    if (surroundedByAir) {
      // Break block
      world.setBlock(x, y, z, 0); // 0 = AIR
      // Drop item? (Not implemented in World yet, but hook exists)
    }
  }
};

#endif
