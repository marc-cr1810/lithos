#ifndef LIQUID_BLOCK_H
#define LIQUID_BLOCK_H

#include "../Block.h"

class LiquidBlock : public Block {
public:
  LiquidBlock(uint8_t id, const std::string &name) : Block(id, name) {}
  bool isSolid() const override { return false; }
  bool isOpaque() const override { return false; }
  RenderLayer getRenderLayer() const override {
    return RenderLayer::TRANSPARENT;
  }

  void getColor(float &r, float &g, float &b) const override {
    if (id == BlockType::WATER) {
      r = 0.2f;
      g = 0.4f;
      b = 1.0f;
    } else if (id == BlockType::LAVA) {
      r = 1.0f;
      g = 0.4f;
      b = 0.0f;
    }
  }

  float getAlpha() const override {
    return 1.0f;
  }

  uint8_t getEmission() const override {
    if (id == BlockType::LAVA)
      return 13;
    return 0;
  }

  void update(World &world, int x, int y, int z) const override;
  void onPlace(World &world, int x, int y, int z) const override;
  void onNeighborChange(World &world, int x, int y, int z, int nx, int ny,
                        int nz) const override;

private:
  void trySpread(World &world, int x, int y, int z, int newMeta) const;
};

#endif
