#include "LavaBlock.h"
#include "../BlockRegistrar.h"

static BlockRegistrar registrar("LavaBlock",
                                [](uint8_t id, const std::string &name) {
                                  return new LavaBlock(id, name);
                                });

LavaBlock::LavaBlock(uint8_t id, const std::string &name)
    : LiquidBlock(id, name) {}

uint8_t LavaBlock::getEmission() const { return 13; }
