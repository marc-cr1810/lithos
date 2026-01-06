#ifndef BLOCK_FACTORY_H
#define BLOCK_FACTORY_H

#include "Block.h"
#include <functional>
#include <map>
#include <memory>
#include <string>

class BlockFactory {
public:
  using BlockConstructor = std::function<Block *(uint8_t, const std::string &)>;

  static BlockFactory &getInstance();

  void registerBlock(const std::string &className,
                     BlockConstructor constructor);
  Block *createBlock(const std::string &className, uint8_t id,
                     const std::string &variantCode);

private:
  BlockFactory(); // Private constructor to enforce singleton and register core
                  // blocks
  std::map<std::string, BlockConstructor> constructors;
};

#endif
