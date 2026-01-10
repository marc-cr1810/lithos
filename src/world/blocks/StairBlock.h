#ifndef STAIR_BLOCK_H
#define STAIR_BLOCK_H

#include "../Block.h"

class StairBlock : public Block {
public:
  StairBlock(block_id id, const std::string &name);
  bool isOpaque() const override;
  bool isSolid() const override;
  RenderShape getRenderShape() const override;

  // Stairs might need rotation based on metadata.
  // We'll rely on Chunk.cpp to interpret metadata for rotation.
  bool isSideSolid(int face, int metadata) const override;
};

#endif
