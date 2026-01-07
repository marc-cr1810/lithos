#ifndef WATER_BLOCK_H
#define WATER_BLOCK_H

#include "LiquidBlock.h"

class WaterBlock : public LiquidBlock {
public:
  WaterBlock(block_id id, const std::string &name);
  uint8_t getLightDecay() const override { return 3; }
};

#endif
