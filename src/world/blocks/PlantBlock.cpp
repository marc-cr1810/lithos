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
