#include "StairBlock.h"
#include "../BlockRegistrar.h"

static BlockRegistrar registrar("StairBlock",
                                [](uint8_t id, const std::string &name) {
                                  return new StairBlock(id, name);
                                });
