#include "BlockBehaviorFactory.h"
#include "../../debug/Logger.h"

BlockBehaviorFactory &BlockBehaviorFactory::getInstance() {
  static BlockBehaviorFactory instance;
  return instance;
}

void BlockBehaviorFactory::registerBehavior(const std::string &name,
                                            BehaviorConstructor constructor) {
  constructors[name] = constructor;
}

std::shared_ptr<BlockBehavior>
BlockBehaviorFactory::createBehavior(const std::string &name, Block *block) {
  auto it = constructors.find(name);
  if (it != constructors.end()) {
    return it->second(block);
  }
  LOG_ERROR("BlockBehaviorFactory: Unknown behavior '{}'", name);
  return nullptr;
}
