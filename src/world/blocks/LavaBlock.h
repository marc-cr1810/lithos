#ifndef LAVA_BLOCK_H
#define LAVA_BLOCK_H

#include "LiquidBlock.h"

class LavaBlock : public LiquidBlock {
public:
  LavaBlock(uint8_t id, const std::string &name) : LiquidBlock(id, name) {}

  void getColor(float &r, float &g, float &b) const override {
    // Lava color
    r = 1.0f;
    g = 0.4f;
    b = 0.0f;
  }

  uint8_t getEmission() const override { return 13; }
};

#endif
