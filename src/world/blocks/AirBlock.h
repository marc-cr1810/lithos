#ifndef AIR_BLOCK_H
#define AIR_BLOCK_H

#include "../Block.h"

class AirBlock : public Block {
public:
  AirBlock() : Block(BlockType::AIR, "Air") {}
  bool isSolid() const override { return false; }
  bool isOpaque() const override { return false; }
  bool isActive() const override { return false; }
  bool isReplaceable() const override { return true; }
};

#endif
