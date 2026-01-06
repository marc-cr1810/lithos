#include "StairBlock.h"
#include "../BlockRegistrar.h"

static BlockRegistrar registrar("StairBlock",
                                [](uint8_t id, const std::string &name) {
                                  return new StairBlock(id, name);
                                });

StairBlock::StairBlock(uint8_t id, const std::string &name) : Block(id, name) {}
bool StairBlock::isOpaque() const { return false; }
bool StairBlock::isSolid() const { return true; }
Block::RenderShape StairBlock::getRenderShape() const {
  return RenderShape::STAIRS;
}
