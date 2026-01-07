#include "LayeredBlock.h"
#include "../BlockRegistrar.h"

static BlockRegistrar registrar("LayeredBlock",
                                [](block_id id, const std::string &name) {
                                  return new LayeredBlock(id, name);
                                });

LayeredBlock::LayeredBlock(block_id id, const std::string &name, int maxLayers)
    : Block(id, name), maxLayers(maxLayers) {}

// Layered blocks are not fully opaque since they don't fill the entire block
// space
bool LayeredBlock::isOpaque() const { return false; }

// But they are solid for collision purposes
bool LayeredBlock::isSolid() const { return true; }

Block::RenderShape LayeredBlock::getRenderShape() const {
  return RenderShape::LAYERED;
}

// Get the height of the block based on layer count in metadata
// metadata 0 = 1 layer (1/8 block), metadata 7 = 8 layers (1.0 block)
float LayeredBlock::getBlockHeight(uint8_t metadata) const {
  int layers = (metadata % maxLayers) + 1; // metadata 0-7 -> 1-8 layers
  return (float)layers / (float)maxLayers;
}

// Get maximum number of layers this block type supports
int LayeredBlock::getMaxLayers() const { return maxLayers; }
