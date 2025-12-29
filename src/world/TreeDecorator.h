#ifndef TREE_DECORATOR_H
#define TREE_DECORATOR_H

#include "WorldDecorator.h"

class TreeDecorator : public WorldDecorator {
public:
  virtual void Decorate(Chunk &chunk, WorldGenerator &generator,
                        const struct ChunkColumn &column) override;
};

#endif
