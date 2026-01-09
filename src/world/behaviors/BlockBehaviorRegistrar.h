#ifndef BLOCK_BEHAVIOR_REGISTRAR_H
#define BLOCK_BEHAVIOR_REGISTRAR_H

#include "BlockBehaviorFactory.h"
#include <string>

class BlockBehaviorRegistrar {
public:
  BlockBehaviorRegistrar(
      const std::string &name,
      BlockBehaviorFactory::BehaviorConstructor constructor) {
    BlockBehaviorFactory::getInstance().registerBehavior(name, constructor);
  }
};

#endif
