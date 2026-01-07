#ifndef FALLING_BLOCK_H
#define FALLING_BLOCK_H

#include "SolidBlock.h"
#include <glm/glm.hpp>

struct ChunkBlock;
class World;

class FallingBlock : public SolidBlock {
public:
  FallingBlock(block_id id, const std::string &name);
  void onPlace(World &world, int x, int y, int z) const override;
  void onNeighborChange(World &world, int x, int y, int z, int nx, int ny,
                        int nz) const override;
  void update(World &world, int x, int y, int z) const override;

private:
  bool canFallThrough(const ChunkBlock &b) const;
};

#endif
