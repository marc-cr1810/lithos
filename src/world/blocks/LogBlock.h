#ifndef LOG_BLOCK_H
#define LOG_BLOCK_H

#include "../Block.h"
#include "SolidBlock.h"

class LogBlock : public SolidBlock {
public:
  LogBlock(uint8_t id, const std::string &name) : SolidBlock(id, name) {}

  void getTextureUV(int faceDir, float &u, float &v, int x, int y, int z,
                    uint8_t metadata, int layer = 0) const override {
    // Metadata 0: Vertical (Y-Axis) - Default
    // Metadata 1: Horizontal X-Axis
    // Metadata 2: Horizontal Z-Axis

    int effectiveFace = faceDir;

    if (metadata == 1) { // X-Axis (East/West)
      // Rotated 90 deg around Z axis conceptually?
      // Side Faces:
      // 0 (Z+), 1 (Z-), 4 (Y+), 5 (Y-) -> BARK
      // 2 (X-), 3 (X+) -> RINGS (Top Texture)

      // We need to swap textures.
      // Default: 4,5 are TOP/BOTTOM (Rings)
      // We want 2,3 to use TOP/BOTTOM Texture
      // We want 4,5 to use SIDE Texture

      if (faceDir == 2 || faceDir == 3) {
        effectiveFace = 4; // Use Top Texture
      } else {
        effectiveFace = 0; // Use Side Texture
      }

    } else if (metadata == 2) { // Z-Axis (North/South)
      // Side Faces:
      // 2 (X-), 3 (X+), 4 (Y+), 5 (Y-) -> BARK
      // 0 (Z+), 1 (Z-) -> RINGS (Top Texture)

      if (faceDir == 0 || faceDir == 1) {
        effectiveFace = 4; // Use Top Texture
      } else {
        effectiveFace = 0; // Use Side Texture
      }

    } else {
      // Default Vertical
      // 4, 5 -> Rings
      // 0, 1, 2, 3 -> Bark
      // Handled by default implementation usually, but since we override all:
      effectiveFace = faceDir;
    }

    // Call base implementation with the "swapped" face direction to get correct
    // UVs from atlas
    Block::getTextureUV(effectiveFace, u, v, x, y, z, metadata, layer);
  }
};

#endif
