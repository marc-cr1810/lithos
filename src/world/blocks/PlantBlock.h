#ifndef PLANT_BLOCK_H
#define PLANT_BLOCK_H

#include "../Block.h"

class PlantBlock : public Block {
public:
  PlantBlock(uint8_t id, const std::string &name);
  bool isSelectable() const override;
  void getColor(float &r, float &g, float &b) const override;
};

#endif
