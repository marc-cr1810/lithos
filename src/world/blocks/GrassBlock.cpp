#include "GrassBlock.h"
#include "../BlockRegistrar.h"

static BlockRegistrar registrar("GrassBlock",
                                [](uint8_t id, const std::string &name) {
                                  return new GrassBlock(id, name);
                                });

GrassBlock::GrassBlock(uint8_t id, const std::string &name)
    : SolidBlock(id, name) {}

void GrassBlock::getColor(float &r, float &g, float &b) const {
  // Green tint for grass
  r = 0.0f;
  g = 1.0f;
  b = 0.0f;
}

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
