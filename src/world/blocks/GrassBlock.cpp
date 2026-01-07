#include "GrassBlock.h"
#include "../BlockRegistrar.h"

static BlockRegistrar registrar("GrassBlock",
                                [](block_id id, const std::string &name) {
                                  return new GrassBlock(id, name);
                                });

GrassBlock::GrassBlock(block_id id, const std::string &name)
    : SolidBlock(id, name) {}

bool GrassBlock::shouldTint(int faceDir, int layer) const {
  // Top face (4) is always tinted
  if (faceDir == 4)
    return true;

  // Side overlay (layer 1) is tinted
  if (layer == 1)
    return true;

  // Base side (dirt) is NOT tinted
  return false;
}
