#include "TallGrassBlock.h"
#include "../Block.h"
#include "../BlockRegistrar.h"

static BlockRegistrar registrar("TallGrassBlock",
                                [](uint8_t id, const std::string &name) {
                                  return new TallGrassBlock(id, name);
                                });

TallGrassBlock::TallGrassBlock(uint8_t id, const std::string &name)
    : PlantBlock(id, name) {}
