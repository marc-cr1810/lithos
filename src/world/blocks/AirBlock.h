#ifndef AIR_BLOCK_H
#define AIR_BLOCK_H

#include "../Block.h"

class AirBlock : public Block {
public:
  AirBlock() : Block(AIR, "Air") { setResourceId("lithos:air"); }
  bool isSolid() const override { return false; }
  bool isOpaque() const override { return false; }
  bool isActive() const override { return false; }
  bool isSelectable() const override { return false; }
  bool isReplaceable() const override { return true; }
  bool isSideSolid(int face, int metadata = 0) const override { return false; }
};

#endif
