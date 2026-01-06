#ifndef BLOCK_REGISTRAR_H
#define BLOCK_REGISTRAR_H

#include "BlockFactory.h"
#include <string>

class BlockRegistrar {
public:
  BlockRegistrar(const std::string &className,
                 BlockFactory::BlockConstructor constructor) {
    BlockFactory::getInstance().registerBlock(className, constructor);
  }
};

#endif // BLOCK_REGISTRAR_H
