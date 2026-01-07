#ifndef LEAVES_BLOCK_H
#define LEAVES_BLOCK_H

#include "PlantBlock.h"

class LeavesBlock : public PlantBlock {
public:
  LeavesBlock(block_id id, const std::string &name);
  bool isLeaves() const override { return true; }
};

#endif
