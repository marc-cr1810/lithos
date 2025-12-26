#ifndef PLANT_BLOCK_H
#define PLANT_BLOCK_H

#include "../Block.h"

class PlantBlock : public Block {
public:
  PlantBlock(uint8_t id, const std::string &name) : Block(id, name) {}
  bool isSolid() const override {
    if (id == BlockType::LEAVES || id == BlockType::PINE_LEAVES)
      return true;
    return false;
  }
  bool isOpaque() const override { return false; }
  RenderLayer getRenderLayer() const override { return RenderLayer::CUTOUT; }

  RenderShape getRenderShape() const override {
    if (id == BlockType::TALL_GRASS || id == BlockType::DEAD_BUSH ||
        id == BlockType::ROSE || id == BlockType::DRY_SHORT_GRASS ||
        id == BlockType::DRY_TALL_GRASS)
      return RenderShape::CROSS;
    return RenderShape::CUBE; // Leaves are cubes
  }

  void getColor(float &r, float &g, float &b) const override {
    if (id == BlockType::LEAVES) {
      r = 0.2f;
      g = 0.8f;
      b = 0.2f;
    } else if (id == BlockType::PINE_LEAVES) {
      r = 0.1f;
      g = 0.4f;
      b = 0.2f;
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
