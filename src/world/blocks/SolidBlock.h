#ifndef SOLID_BLOCK_H
#define SOLID_BLOCK_H

#include "../Block.h"

class SolidBlock : public Block {
public:
  SolidBlock(block_id id, const std::string &name);
  RenderLayer getRenderLayer() const override;
  bool shouldTint(int faceDir, int layer) const override;
};

#endif
