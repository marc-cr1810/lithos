#include "SolidBlock.h"
#include "../BlockRegistrar.h"

static BlockRegistrar registrar("SolidBlock",
                                [](uint8_t id, const std::string &name) {
                                  return new SolidBlock(id, name);
                                });
