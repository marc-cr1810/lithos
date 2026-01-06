#ifndef WATER_BLOCK_H
#define WATER_BLOCK_H

#include "LiquidBlock.h"

class WaterBlock : public LiquidBlock {
public:
  WaterBlock(uint8_t id, const std::string &name) : LiquidBlock(id, name) {}

  void getColor(float &r, float &g, float &b) const override {
    // Water color
    r = 0.2f;
    g = 0.4f;
    b = 1.0f;
  }
};

#endif
