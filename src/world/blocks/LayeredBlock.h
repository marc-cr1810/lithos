#ifndef LAYERED_BLOCK_H
#define LAYERED_BLOCK_H

#include "../Block.h"

class LayeredBlock : public Block {
public:
  LayeredBlock(uint8_t id, const std::string &name, int maxLayers = 8);
  bool isOpaque() const override;
  bool isSolid() const override;
  RenderShape getRenderShape() const override;
  float getBlockHeight(uint8_t metadata) const override;
  int getMaxLayers() const;

private:
  int maxLayers;
};

#endif
