#ifndef LEAVES_BLOCK_H
#define LEAVES_BLOCK_H

#include "PlantBlock.h"

class LeavesBlock : public PlantBlock {
public:
  LeavesBlock(uint8_t id, const std::string &name) : PlantBlock(id, name) {
    // Leaves are solid (spawnable/collidable usually) but use cutout rendering
    isSolid_ = true;
    renderShape = RenderShape::CUBE;
    // renderLayer is inherited as CUTOUT from PlantBlock default, which is
    // correct
  }

  void getColor(float &r, float &g, float &b) const override {
    // Simplified leaf colors based on hardcoded ID mapping for now.
    // Ideally this should be data-driven or biome-driven.
    // Using explicit checks here is still somewhat hardcoded but localized to
    // this class.

    // We can't easily avoid ID checks entirely without adding a "foliageType"
    // property or constructor arg. For now, we replicate the ID logic but
    // inside this specific class.

    if (id == BlockType::LEAVES) { // Oak
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
    } else {
      // Fallback
      r = 0.2f;
      g = 0.8f;
      b = 0.2f;
    }
  }
};

#endif
