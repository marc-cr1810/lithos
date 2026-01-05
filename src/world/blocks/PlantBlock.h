#ifndef PLANT_BLOCK_H
#define PLANT_BLOCK_H

#include "../Block.h"

class PlantBlock : public Block {
public:
  PlantBlock(uint8_t id, const std::string &name) : Block(id, name) {
    // defaults for plants
    isOpaque_ = false;
    isSolid_ = false;
    isReplaceable_ = true;
    renderLayer = RenderLayer::CUTOUT;
    renderShape = RenderShape::CROSS;

    if (id == BlockType::LEAVES || id == BlockType::SPRUCE_LEAVES ||
        id == BlockType::ACACIA_LEAVES || id == BlockType::BIRCH_LEAVES ||
        id == BlockType::DARK_OAK_LEAVES || id == BlockType::JUNGLE_LEAVES) {
      isSolid_ = true;
      renderShape = RenderShape::CUBE;
    }
    // Other plants stay CROSS/non-solid
  }

  bool isSelectable() const override { return true; }
  // bool isReplaceable() const override { return true; } // Handled by
  // isReplaceable_

  void getColor(float &r, float &g, float &b) const override {
    if (id == BlockType::LEAVES) {
      r = 0.2f;
      g = 0.8f;
      b = 0.2f;
    } else if (id == BlockType::SPRUCE_LEAVES) {
      r = 0.1f;
      g = 0.4f;
      b = 0.2f;
    } else if (id == BlockType::BIRCH_LEAVES) {
      r = 0.3f;
      g = 0.65f;
      b = 0.3f;
    } else if (id == BlockType::JUNGLE_LEAVES) {
      r = 0.2f;
      g = 0.9f;
      b = 0.2f;
    } else if (id == BlockType::ACACIA_LEAVES) {
      r = 0.4f;
      g = 0.7f;
      b = 0.2f;
    } else if (id == BlockType::DARK_OAK_LEAVES) {
      r = 0.1f;
      g = 0.35f;
      b = 0.1f;
    } else if (id == BlockType::TALL_GRASS) {
      r = 0.2f;
      g = 0.8f;
      b = 0.2f; // Match Leaves/Grass tint
    } else {
      // Plants: Use Texture Color (White Tint)
      r = 1.0f;
      g = 1.0f;
      b = 1.0f;
    }
  }
};

#endif
