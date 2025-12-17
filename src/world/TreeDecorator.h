#ifndef TREE_DECORATOR_H
#define TREE_DECORATOR_H

#include "WorldDecorator.h"

class TreeDecorator : public WorldDecorator
{
public:
    void Decorate(Chunk& chunk, WorldGenerator& generator) override;
};

#endif
