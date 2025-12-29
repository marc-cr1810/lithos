#ifndef WORLD_DECORATOR_H
#define WORLD_DECORATOR_H

#include "Chunk.h"

class WorldGenerator;

class WorldDecorator {
public:
  virtual ~WorldDecorator() {}
  virtual void Decorate(Chunk &chunk, WorldGenerator &generator,
                        const struct ChunkColumn &column) = 0;
};

#endif
