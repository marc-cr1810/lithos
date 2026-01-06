#include "LeavesBlock.h"
#include "../BlockRegistrar.h"

static BlockRegistrar registrar("LeavesBlock",
                                [](uint8_t id, const std::string &name) {
                                  return new LeavesBlock(id, name);
                                });
