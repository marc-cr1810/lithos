#ifndef PLANT_BLOCK_H
#define PLANT_BLOCK_H

#include "../Block.h"

class PlantBlock : public Block {
public:
  PlantBlock(block_id id, const std::string &name);
  bool isSelectable() const override;
};

#endif
