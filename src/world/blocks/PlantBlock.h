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
        id == BlockType::ROSE)
      return RenderShape::CROSS;
    return RenderShape::CUBE; // Leaves are cubes
  }

  void getTextureUV(int faceDir, float &uMin, float &vMin) const override {
    if (id == BlockType::LEAVES) {
      uMin = 0.50f;
      vMin = 0.125f; // Was 0.25
    } else if (id == BlockType::PINE_LEAVES) {
      uMin = 0.50f;
      vMin = 0.125f; // Was 0.25
    } // Reuse Oak leaves for now but darker
    else if (id == BlockType::TALL_GRASS) {
      uMin = 0.50f;
      vMin = 0.50f; // Slot (2, 4) -> 4*0.125 = 0.5
    } else if (id == BlockType::DEAD_BUSH) {
      uMin = 0.75f;
      vMin = 0.50f; // Slot (3, 4)
    } // Dead bush
    else if (id == BlockType::ROSE) {
      uMin = 0.00f;
      vMin = 0.625f; // Slot (0, 5) -> 5*0.125 = 0.625
    } else {
      uMin = 0.50f;
      vMin = 0.125f;
    }
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
    } // Darker
    else {
      // Plants: Use Texture Color (White Tint)
      r = 1.0f;
      g = 1.0f;
      b = 1.0f;
    }
  }
};

#endif
