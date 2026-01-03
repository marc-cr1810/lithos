#ifndef ORE_DECORATOR_H
#define ORE_DECORATOR_H

#include "WorldDecorator.h"

class OreDecorator : public WorldDecorator {
public:
  virtual void Decorate(Chunk &chunk, WorldGenerator &generator,
                        const struct ChunkColumn &column) override;

  virtual void Decorate(WorldGenerator &generator, WorldGenRegion &region,
                        const struct ChunkColumn &column) override;

private:
  void GenerateOre(Chunk &chunk, int startX, int startY, int startZ,
                   BlockType oreType);
};

#endif
