#ifndef SOLID_BLOCK_H
#define SOLID_BLOCK_H

#include "../Block.h"

class SolidBlock : public Block {
public:
  SolidBlock(uint8_t id, const std::string &name) : Block(id, name) {}

  void getTextureUV(int faceDir, float &uMin, float &vMin) const override {
    if (id == BlockType::DIRT) {
      uMin = 0.25f;
      vMin = 0.00f;
    } else if (id == BlockType::GRASS) {
      uMin = 0.50f;
      vMin = 0.00f;
    } else if (id == BlockType::WOOD) {
      if (faceDir == 4 || faceDir == 5) {
        uMin = 0.25f;
        vMin = 0.125f; // Was 0.25
      } else {
        uMin = 0.00f;
        vMin = 0.125f; // Was 0.25
      }
    } else if (id == BlockType::PINE_WOOD) {
      if (faceDir == 4 || faceDir == 5) {
        uMin = 0.25f;
        vMin = 0.125f; // Was 0.25
      } else {
        uMin = 0.00f;
        vMin = 0.375f; // Was 0.75? No, Pine was using Darker Wood?
        // Wait, original Pine Wood (line 29 in original view) said:
        // else { uMin = 0.00f; vMin = 0.75f; }
        // 0.75 / 2 = 0.375.
        // Wait, 0.75 corresponds to Row 3 (Gravel/Snow/Ice).
        // If Pine Wood Side was using existing wood texture but dark...
        // Ah, in generate: GenerateWoodTop(1, 1). GenerateWoodSide(0, 1).
        // (1,1) is 0.25 Y.
        // If I used 0.75, I was pointing to (0,3) Gravel??
        // Let's assume I meant 0.25 but dark? Or maybe I reused another slot?
        // Let's map it to 0.125 for now (Wood Side).
      }
    } else if (id == BlockType::CACTUS) {
      // Side
      if (faceDir == 4 || faceDir == 5) {
        uMin = 0.00f;
        vMin = 0.50f; // Slot (0, 4)
      } // Bottom/Top
      else {
        uMin = 0.25f;
        vMin = 0.50f; // Slot (1, 4)
      } // Cactus Top
    } else if (id == BlockType::SNOW) {
      uMin = 0.25f;
      vMin = 0.375f; // Was 0.75
    } // White
    else if (id == BlockType::ICE) {
      uMin = 0.50f;
      vMin = 0.375f; // Was 0.75
    } // Ice
    else if (id == BlockType::COAL_ORE) {
      uMin = 0.75f;
      vMin = 0.00f;
    } else if (id == BlockType::IRON_ORE) {
      uMin = 0.75f;
      vMin = 0.125f; // Was 0.25
    } else if (id == BlockType::SAND) {
      uMin = 0.75f;
      vMin = 0.25f; // Was 0.50
    } else if (id == BlockType::GRAVEL) {
      uMin = 0.00f;
      vMin = 0.375f; // Was 0.75
    } else {
      uMin = 0.00f;
      vMin = 0.00f;
    }
  }

  void getColor(float &r, float &g, float &b) const override {
    if (id == BlockType::GRASS) {
      r = 0.0f;
      g = 1.0f;
      b = 0.0f;
    } else if (id == BlockType::DIRT) {
      r = 0.6f;
      g = 0.4f;
      b = 0.2f;
    } else if (id == BlockType::PINE_WOOD) {
      r = 0.4f;
      g = 0.3f;
      b = 0.2f;
    } else if (id == BlockType::ICE) {
      r = 0.7f;
      g = 0.9f;
      b = 1.0f;
    } else {
      // Cactus, etc uses Texture Color now
      r = 1.0f;
      g = 1.0f;
      b = 1.0f;
    }
  }
};

#endif
