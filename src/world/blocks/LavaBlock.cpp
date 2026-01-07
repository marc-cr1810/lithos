#include "LavaBlock.h"
#include "../BlockRegistrar.h"

static BlockRegistrar registrar("LavaBlock",
                                [](block_id id, const std::string &name) {
                                  return new LavaBlock(id, name);
                                });

LavaBlock::LavaBlock(block_id id, const std::string &name)
    : LiquidBlock(id, name) {}

uint8_t LavaBlock::getEmission() const { return 13; }
