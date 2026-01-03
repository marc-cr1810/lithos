#ifndef FLORA_DECORATOR_H
#define FLORA_DECORATOR_H

#include "WorldDecorator.h"

class FloraDecorator : public WorldDecorator {
public:
  virtual void Decorate(Chunk &chunk, WorldGenerator &generator,
                        const struct ChunkColumn &column) override;

  virtual void Decorate(WorldGenerator &generator, WorldGenRegion &region,
                        const struct ChunkColumn &column) override;
};

#endif
