#ifndef BLOCK_BEHAVIOR_FINITE_SPREADING_LIQUID_H
#define BLOCK_BEHAVIOR_FINITE_SPREADING_LIQUID_H

#include "BlockBehavior.h"
#include "BlockBehaviorFactory.h"
#include <vector>

class BlockBehaviorFiniteSpreadingLiquid : public BlockBehavior {
public:
  BlockBehaviorFiniteSpreadingLiquid(Block *block);

  void onLoaded(const nlohmann::json &properties) override;
  void onPlace(World &world, int x, int y, int z) const override;
  void onNeighborChange(World &world, int x, int y, int z, int nx, int ny,
                        int nz) const override;
  void update(World &world, int x, int y, int z) const override;

private:
  int spreadDelay = 100;
  std::string liquidCollisionSound;
  std::string sourceReplacementCode;
  std::string flowingReplacementCode;
  std::vector<std::string> collidesWith;

  void trySpread(World &world, int x, int y, int z) const;
  bool canSpreadInto(World &world, int x, int y, int z, int level) const;
  block_id getLiquidBlockId(const std::string &baseCode,
                            const std::string &flowDir, int level) const;
};

#endif
