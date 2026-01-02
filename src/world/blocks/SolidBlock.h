#ifndef SOLID_BLOCK_H
#define SOLID_BLOCK_H

#include "../Block.h"

class SolidBlock : public Block {
public:
  SolidBlock(uint8_t id, const std::string &name) : Block(id, name) {}

  RenderLayer getRenderLayer() const override {
    if (id == BlockType::ICE)
      return RenderLayer::TRANSPARENT;
    // Transparent leaves check? Leaves usually use CUTOUT or OPAQUE
    // (Fast/Fancy) For now standard SolidBlock is opaque.
    return RenderLayer::OPAQUE;
  }

  void getColor(float &r, float &g, float &b) const override {
    if (id == BlockType::GRASS) {
      r = 0.0f;
      g = 1.0f;
      b = 0.0f; // Green Tint for top?
      // Wait, original grass texture is grey?
      // grass_block_top.png usually is grey in MC, tinted by biome.
      // If the PNG is already green, we should return white.
      // I don't know the PNG content. Let's assume we need tint for now or
      // white. If I look at the file list, typically in MC top is grey. But
      // let's check if the user said anything? No. I'll keep the tint for
      // Grass.
    } else if (id == BlockType::DIRT) {
      // Dirt texture usually brown. Tinting it might make it too dark if
      // texture is already brown. Original code tinted dirt (0.6, 0.4, 0.2).
      // Real MC dirt texture is brown. We should probably NOT tint it if we
      // have a texture. So I will remove DIRT tint.
      r = 1.0f;
      g = 1.0f;
      b = 1.0f;
    } else if (id == BlockType::SPRUCE_LOG) {
      // Original tinted pine wood.
      // We have spruce_log.png. We should use it directly.
      r = 1.0f;
      g = 1.0f;
      b = 1.0f;
    } else if (id == BlockType::ICE) {
      // Ice tint check?
      // ice.png exists.
      r = 1.0f;
      g = 1.0f;
      b = 1.0f;
    } else {
      r = 1.0f;
      g = 1.0f;
      b = 1.0f;
    }
  }

  bool shouldTint(int faceDir, int layer) const override {
    if (id == BlockType::GRASS) {
      if (layer == 1)
        return true; // Overlay is tinted
      if (faceDir == 4)
        return true; // Top is tinted
      return false;  // Side base (Dirt) is NOT tinted
    }
    return false; // Default: No tint for dirt, wood, etc.
  }
};

#endif
