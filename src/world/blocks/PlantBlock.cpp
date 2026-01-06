#include "PlantBlock.h"
#include "../BlockRegistrar.h"

static BlockRegistrar registrar("PlantBlock",
                                [](uint8_t id, const std::string &name) {
                                  return new PlantBlock(id, name);
                                });

PlantBlock::PlantBlock(uint8_t id, const std::string &name) : Block(id, name) {
  // defaults for plants (cross shape, cutout, non-solid)
  isOpaque_ = false;
  isSolid_ = false;
  isReplaceable_ = true;
  renderLayer = RenderLayer::CUTOUT;
  renderShape = RenderShape::CROSS;
}

bool PlantBlock::isSelectable() const { return true; }

void PlantBlock::getColor(float &r, float &g, float &b) const {
  // Generic plants use white tint (texture color)
  r = 1.0f;
  g = 1.0f;
  b = 1.0f;
}
