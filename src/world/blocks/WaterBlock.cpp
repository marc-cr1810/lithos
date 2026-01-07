#include "WaterBlock.h"
#include "../BlockRegistrar.h"

static BlockRegistrar registrar("WaterBlock",
                                [](block_id id, const std::string &name) {
                                  return new WaterBlock(id, name);
                                });

WaterBlock::WaterBlock(block_id id, const std::string &name)
    : LiquidBlock(id, name) {}
