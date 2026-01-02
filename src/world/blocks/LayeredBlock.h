#ifndef LAYERED_BLOCK_H
#define LAYERED_BLOCK_H

#include "../Block.h"

class LayeredBlock : public Block {
public:
  LayeredBlock(uint8_t id, const std::string &name, int maxLayers = 8)
      : Block(id, name), maxLayers(maxLayers) {}

  // Layered blocks are not fully opaque since they don't fill the entire block
  // space
  bool isOpaque() const override { return false; }

  // But they are solid for collision purposes
  bool isSolid() const override { return true; }

  RenderShape getRenderShape() const override { return RenderShape::LAYERED; }

  // Get the height of the block based on layer count in metadata
  // metadata 0 = 1 layer (1/8 block), metadata 7 = 8 layers (1.0 block)
  float getBlockHeight(uint8_t metadata) const override {
    int layers = (metadata % maxLayers) + 1; // metadata 0-7 -> 1-8 layers
    return (float)layers / (float)maxLayers;
  }

  // Get maximum number of layers this block type supports
  int getMaxLayers() const { return maxLayers; }

private:
  int maxLayers;
};

#endif
