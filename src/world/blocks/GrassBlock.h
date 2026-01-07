#ifndef GRASS_BLOCK_H
#define GRASS_BLOCK_H

#include "SolidBlock.h"

class GrassBlock : public SolidBlock {
public:
  GrassBlock(block_id id, const std::string &name);
  bool shouldTint(int faceDir, int layer) const override;
};

#endif
