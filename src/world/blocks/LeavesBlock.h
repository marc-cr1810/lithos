#ifndef LEAVES_BLOCK_H
#define LEAVES_BLOCK_H

#include "PlantBlock.h"

class LeavesBlock : public PlantBlock {
public:
  LeavesBlock(uint8_t id, const std::string &name);
};

#endif
