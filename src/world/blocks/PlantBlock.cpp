#include "PlantBlock.h"
#include "../BlockRegistrar.h"

static BlockRegistrar registrar("PlantBlock",
                                [](uint8_t id, const std::string &name) {
                                  return new PlantBlock(id, name);
                                });
