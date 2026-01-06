#ifndef TALL_GRASS_BLOCK_H
#define TALL_GRASS_BLOCK_H

#include "PlantBlock.h"

class TallGrassBlock : public PlantBlock {
public:
  TallGrassBlock(uint8_t id, const std::string &name);
  void getColor(float &r, float &g, float &b) const override;
};

#endif
