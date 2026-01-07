#ifndef LAVA_BLOCK_H
#define LAVA_BLOCK_H

#include "LiquidBlock.h"

class LavaBlock : public LiquidBlock {
public:
  LavaBlock(block_id id, const std::string &name);
  uint8_t getEmission() const override;
};

#endif
