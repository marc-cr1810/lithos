#ifndef BLOCK_BEHAVIOR_FACTORY_H
#define BLOCK_BEHAVIOR_FACTORY_H

#include "BlockBehavior.h"
#include <functional>
#include <map>
#include <memory>
#include <string>

class BlockBehaviorFactory {
public:
  using BehaviorConstructor =
      std::function<std::shared_ptr<BlockBehavior>(Block *)>;

  static BlockBehaviorFactory &getInstance();

  void registerBehavior(const std::string &name,
                        BehaviorConstructor constructor);
  std::shared_ptr<BlockBehavior> createBehavior(const std::string &name,
                                                Block *block);

private:
  BlockBehaviorFactory() = default;
  std::map<std::string, BehaviorConstructor> constructors;
};

#endif
