#ifndef WORLDGENERATOR_H
#define WORLDGENERATOR_H

#include <vector>

class Chunk;
class WorldDecorator;

class WorldGenerator {
public:
  WorldGenerator(int seed);
  ~WorldGenerator();
  void GenerateChunk(Chunk &chunk);
  int GetHeight(int x, int z); // Converted to Instance Method

private:
  std::vector<WorldDecorator *> decorators;
  int seed;
};

#endif
