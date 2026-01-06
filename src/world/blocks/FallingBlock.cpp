#include "FallingBlock.h"
#include "../BlockRegistrar.h"

static BlockRegistrar registrar("FallingBlock",
                                [](uint8_t id, const std::string &name) {
                                  return new FallingBlock(id, name);
                                });
