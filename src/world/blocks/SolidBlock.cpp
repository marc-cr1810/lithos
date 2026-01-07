#include "SolidBlock.h"
#include "../BlockRegistrar.h"

static BlockRegistrar registrar("SolidBlock",
                                [](uint8_t id, const std::string &name) {
                                  return new SolidBlock(id, name);
                                });

SolidBlock::SolidBlock(uint8_t id, const std::string &name) : Block(id, name) {}

Block::RenderLayer SolidBlock::getRenderLayer() const {
  // Fast/Fancy considerations would go here, or handled by specific subclass
  // (like IceBlock or LeavesBlock) By default, SolidBlocks are OPAQUE.
  return RenderLayer::OPAQUE;
}

bool SolidBlock::shouldTint(int faceDir, int layer) const {
  return false; // Default: No tint for dirt, wood, etc.
}
