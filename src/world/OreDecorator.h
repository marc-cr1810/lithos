#ifndef ORE_DECORATOR_H
#define ORE_DECORATOR_H

#include "WorldDecorator.h"

class OreDecorator : public WorldDecorator
{
public:
    void Decorate(Chunk& chunk, WorldGenerator& generator) override;

private:
    void GenerateOre(Chunk& chunk, int startX, int startY, int startZ, BlockType oreType);
};

#endif
