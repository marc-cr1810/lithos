#ifndef SOLID_BLOCK_H
#define SOLID_BLOCK_H

#include "../Block.h"

class SolidBlock : public Block {
public:
  SolidBlock(uint8_t id, const std::string &name) : Block(id, name) {}

  RenderLayer getRenderLayer() const override {
    // Fast/Fancy considerations would go here, or handled by specific subclass
    // (like IceBlock or LeavesBlock) By default, SolidBlocks are OPAQUE.
    return RenderLayer::OPAQUE;
  }

  void getColor(float &r, float &g, float &b) const override {
    r = 1.0f;
    g = 1.0f;
    b = 1.0f;
  }

  bool shouldTint(int faceDir, int layer) const override {
    return false; // Default: No tint for dirt, wood, etc.
  }
};

#endif
