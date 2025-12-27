#ifndef STAIR_BLOCK_H
#define STAIR_BLOCK_H

#include "../Block.h"

class StairBlock : public Block {
public:
  StairBlock(uint8_t id, const std::string &name) : Block(id, name) {}

  bool isOpaque() const override { return false; }
  bool isSolid() const override { return true; }

  RenderShape getRenderShape() const override { return RenderShape::STAIRS; }

  // Stairs might need rotation based on metadata.
  // We'll rely on Chunk.cpp to interpret metadata for rotation.
};

#endif
