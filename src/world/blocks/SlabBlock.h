#ifndef SLAB_BLOCK_H
#define SLAB_BLOCK_H

#include "../Block.h"

class SlabBlock : public Block {
public:
  SlabBlock(uint8_t id, const std::string &name);
  bool isOpaque() const override;
  bool isSolid() const override;
  RenderShape getRenderShape() const override;
};

#endif
