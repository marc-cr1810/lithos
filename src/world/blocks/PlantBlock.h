#ifndef PLANT_BLOCK_H
#define PLANT_BLOCK_H

#include "../Block.h"

class PlantBlock : public Block {
public:
  PlantBlock(uint8_t id, const std::string &name) : Block(id, name) {
    // defaults for plants (cross shape, cutout, non-solid)
    isOpaque_ = false;
    isSolid_ = false;
    isReplaceable_ = true;
    renderLayer = RenderLayer::CUTOUT;
    renderShape = RenderShape::CROSS;
  }

  bool isSelectable() const override { return true; }

  void getColor(float &r, float &g, float &b) const override {
    // Generic plants use white tint (texture color)
    r = 1.0f;
    g = 1.0f;
    b = 1.0f;
  }
};

#endif
