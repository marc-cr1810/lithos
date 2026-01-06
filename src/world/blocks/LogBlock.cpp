#include "LogBlock.h"
#include "../BlockRegistrar.h"

static BlockRegistrar registrar("LogBlock",
                                [](uint8_t id, const std::string &name) {
                                  return new LogBlock(id, name);
                                });
