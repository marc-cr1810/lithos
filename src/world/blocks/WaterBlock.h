#ifndef WATER_BLOCK_H
#define WATER_BLOCK_H

#include "LiquidBlock.h"

class WaterBlock : public LiquidBlock {
public:
  WaterBlock(uint8_t id, const std::string &name);
};

#endif
