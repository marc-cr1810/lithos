#ifndef GRASS_BLOCK_H
#define GRASS_BLOCK_H

#include "SolidBlock.h"

class GrassBlock : public SolidBlock {
public:
  GrassBlock(uint8_t id, const std::string &name) : SolidBlock(id, name) {}

  void getColor(float &r, float &g, float &b) const override {
    // Green tint for grass
    r = 0.0f;
    g = 1.0f;
    b = 0.0f;
  }

  bool shouldTint(int faceDir, int layer) const override {
    // Top face (4) is always tinted
    if (faceDir == 4)
      return true;

    // Side overlay (layer 1) is tinted
    if (layer == 1)
      return true;

    // Base side (dirt) is NOT tinted
    return false;
  }
};

#endif
