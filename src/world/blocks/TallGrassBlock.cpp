#include "TallGrassBlock.h"
#include "../Block.h"
#include "../BlockRegistrar.h"

static BlockRegistrar registrar("TallGrassBlock",
                                [](uint8_t id, const std::string &name) {
                                  return new TallGrassBlock(id, name);
                                });

TallGrassBlock::TallGrassBlock(uint8_t id, const std::string &name)
    : PlantBlock(id, name) {}

void TallGrassBlock::getColor(float &r, float &g, float &b) const {
  if (name.find("tall_green") != std::string::npos) { // Tall Green Grass
    r = 0.0f;
    g = 1.0f;
    b = 0.0f;
  } else {
    // Dry grass has its own texture color, no tint needed
    r = 1.0f;
    g = 1.0f;
    b = 1.0f;
  }
}
