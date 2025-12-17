#ifndef WORLDGENERATOR_H
#define WORLDGENERATOR_H

class Chunk;

class WorldGenerator
{
public:
    WorldGenerator();
    void GenerateChunk(Chunk& chunk);
    static int GetHeight(int x, int z);
};

#endif
