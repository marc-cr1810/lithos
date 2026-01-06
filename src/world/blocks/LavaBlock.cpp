#include "LavaBlock.h"
#include "../BlockRegistrar.h"

static BlockRegistrar registrar("LavaBlock",
                                [](uint8_t id, const std::string &name) {
                                  return new LavaBlock(id, name);
                                });

LavaBlock::LavaBlock(uint8_t id, const std::string &name)
    : LiquidBlock(id, name) {}

void LavaBlock::getColor(float &r, float &g, float &b) const {
  // Lava color
  r = 1.0f;
  g = 0.4f;
  b = 0.0f;
}

uint8_t LavaBlock::getEmission() const { return 13; }
