#include "LavaBlock.h"
#include "../BlockRegistrar.h"

static BlockRegistrar registrar("LavaBlock",
                                [](uint8_t id, const std::string &name) {
                                  return new LavaBlock(id, name);
                                });
