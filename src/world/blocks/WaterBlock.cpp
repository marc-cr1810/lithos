#include "WaterBlock.h"
#include "../BlockRegistrar.h"

static BlockRegistrar registrar("WaterBlock",
                                [](uint8_t id, const std::string &name) {
                                  return new WaterBlock(id, name);
                                });
