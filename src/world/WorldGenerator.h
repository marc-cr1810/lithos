#ifndef WORLDGENERATOR_H
#define WORLDGENERATOR_H

#include <vector>

class Chunk;
class WorldDecorator;

class WorldGenerator
{
public:
    WorldGenerator();
    ~WorldGenerator();
    void GenerateChunk(Chunk& chunk);
    static int GetHeight(int x, int z);

private:
    std::vector<WorldDecorator*> decorators;
};

#endif
