#include "BlockBehaviorBreakIfFloating.h"
#include "../World.h"
#include "BlockBehaviorRegistrar.h"

// Register the behavior
static BlockBehaviorRegistrar registrar("BreakIfFloating", [](Block *block) {
  return std::make_shared<BlockBehaviorBreakIfFloating>(block);
});

void BlockBehaviorBreakIfFloating::onNeighborChange(World &world, int x, int y,
                                                    int z, int nx, int ny,
                                                    int nz) const {
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
