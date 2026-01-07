#ifndef LIQUID_BLOCK_H
#define LIQUID_BLOCK_H

#include "../Block.h"

class LiquidBlock : public Block {
public:
  LiquidBlock(block_id id, const std::string &name);
  bool isSolid() const override;
  bool isReplaceable() const override;
  bool isOpaque() const override;
  RenderLayer getRenderLayer() const override;
  float getAlpha() const override;
  uint8_t getEmission() const override;
  bool isLiquid() const override { return true; }

  void update(World &world, int x, int y, int z) const override;
  void onPlace(World &world, int x, int y, int z) const override;
  void onNeighborChange(World &world, int x, int y, int z, int nx, int ny,
                        int nz) const override;

private:
  void trySpread(World &world, int x, int y, int z, int newMeta) const;
  void checkMixing(World &world, int x, int y, int z) const;
};

#endif
