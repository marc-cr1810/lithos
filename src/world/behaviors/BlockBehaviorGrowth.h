#ifndef BLOCK_BEHAVIOR_GROWTH_H
#define BLOCK_BEHAVIOR_GROWTH_H

#include "BlockBehavior.h"
#include <random>

class BlockBehaviorGrowth : public BlockBehavior {
public:
  BlockBehaviorGrowth(Block *block);

  void onLoaded(const nlohmann::json &properties) override;
  void onRandomTick(World &world, int x, int y, int z,
                    std::mt19937 &rng) const override;

private:
  int maxStage = 1;
  float growthChance = 0.5f;
  std::string nextBlock;
};

#endif
