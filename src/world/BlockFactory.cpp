#include "BlockFactory.h"
#include "../debug/Logger.h"
#include "blocks/SolidBlock.h"

BlockFactory &BlockFactory::getInstance() {
  static BlockFactory instance;
  return instance;
}

BlockFactory::BlockFactory() {
  // Blocks now self-register via BlockRegistrar in their respective .cpp files
}

void BlockFactory::registerBlock(const std::string &className,
                                 BlockConstructor constructor) {
  constructors[className] = constructor;
}

Block *BlockFactory::createBlock(const std::string &className, uint8_t id,
                                 const std::string &variantCode) {
  auto it = constructors.find(className);
  if (it != constructors.end()) {
    return it->second(id, variantCode);
  }
  LOG_ERROR("BlockFactory: Unknown block class '{}' for block '{}'. Defaulting "
            "to SolidBlock.",
            className, variantCode);
  return new SolidBlock(id, variantCode);
}
