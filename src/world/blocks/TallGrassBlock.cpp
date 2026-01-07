#include "TallGrassBlock.h"
#include "../Block.h"
#include "../BlockRegistrar.h"

static BlockRegistrar registrar("TallGrassBlock",
                                [](block_id id, const std::string &name) {
                                  return new TallGrassBlock(id, name);
                                });

TallGrassBlock::TallGrassBlock(block_id id, const std::string &name)
    : PlantBlock(id, name) {}
