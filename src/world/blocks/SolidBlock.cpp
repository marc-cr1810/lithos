#include "SolidBlock.h"
#include "../BlockRegistrar.h"

static BlockRegistrar registrar("SolidBlock",
                                [](block_id id, const std::string &name) {
                                  return new SolidBlock(id, name);
                                });

SolidBlock::SolidBlock(block_id id, const std::string &name)
    : Block(id, name) {}

Block::RenderLayer SolidBlock::getRenderLayer() const {
  // Fast/Fancy considerations would go here, or handled by specific subclass
  // (like IceBlock or LeavesBlock) By default, SolidBlocks are OPAQUE.
  return RenderLayer::OPAQUE;
}
