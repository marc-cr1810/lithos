#include "WaterBlock.h"
#include "../BlockRegistrar.h"

static BlockRegistrar registrar("WaterBlock",
                                [](uint8_t id, const std::string &name) {
                                  return new WaterBlock(id, name);
                                });

WaterBlock::WaterBlock(uint8_t id, const std::string &name)
    : LiquidBlock(id, name) {}

void WaterBlock::getColor(float &r, float &g, float &b) const {
  // Water color
  r = 0.2f;
  g = 0.4f;
  b = 1.0f;
}
