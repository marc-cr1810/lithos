#include "SlabBlock.h"
#include "../BlockRegistrar.h"

static BlockRegistrar registrar("SlabBlock",
                                [](uint8_t id, const std::string &name) {
                                  return new SlabBlock(id, name);
                                });
