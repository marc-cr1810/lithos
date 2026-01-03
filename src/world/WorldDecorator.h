#ifndef WORLD_DECORATOR_H
#define WORLD_DECORATOR_H

#include "Chunk.h"

class WorldGenerator;

class WorldDecorator {
public:
  virtual ~WorldDecorator() {}
  // Chunk-based decoration
  virtual void Decorate(Chunk &chunk, WorldGenerator &generator,
                        const struct ChunkColumn &column) = 0;

  // Region-based decoration (for cross-chunk features)
  virtual void Decorate(WorldGenerator &generator, class WorldGenRegion &region,
                        const struct ChunkColumn &column) {}
};

#endif
