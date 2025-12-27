#ifndef SLAB_BLOCK_H
#define SLAB_BLOCK_H

#include "../Block.h"

class SlabBlock : public Block {
public:
  SlabBlock(uint8_t id, const std::string &name) : Block(id, name) {}

  // Slabs are not full opaque cubes, so they don't occlude neighbors fully in
  // all directions. Actually, a bottom slab DOES occlude the face below it, but
  // for simplicity in Greedy Meshing / AO, we might mark it as non-opaque so
  // the system doesn't try to optimize it away or occlude others wrongly. If we
  // mark it opaque, we need to ensure occlusion logic handles half-height. The
  // simplest "custom mesh" approach is to treat it as transparent/non-opaque
  // for occlusion purposes so that faces behind it are rendered.
  bool isOpaque() const override { return false; }

  // But we want it to be solid for collision (when collision is implemented)
  bool isSolid() const override { return true; }

  RenderShape getRenderShape() const override {
    return RenderShape::SLAB_BOTTOM;
  }
};

#endif
