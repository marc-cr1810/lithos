#include "GrassBlock.h"
#include "../BlockRegistrar.h"

static BlockRegistrar registrar("GrassBlock",
                                [](uint8_t id, const std::string &name) {
                                  return new GrassBlock(id, name);
                                });
