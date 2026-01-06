#include "LayeredBlock.h"
#include "../BlockRegistrar.h"

static BlockRegistrar registrar("LayeredBlock",
                                [](uint8_t id, const std::string &name) {
                                  return new LayeredBlock(id, name);
                                });
