#ifndef WORLDGENERATOR_H
#define WORLDGENERATOR_H

#include <vector>

class Chunk;
class WorldDecorator;

enum Biome {
  BIOME_OCEAN,
  BIOME_BEACH,
  BIOME_DESERT,
  BIOME_TUNDRA,
  BIOME_FOREST,
  BIOME_PLAINS
};

class WorldGenerator {
public:
  WorldGenerator(int seed);
  ~WorldGenerator();
  void GenerateChunk(Chunk &chunk);
  int GetHeight(int x, int z); // Converted to Instance Method
  float GetTemperature(int x, int z);
  float GetHumidity(int x, int z);
  Biome GetBiome(int x, int z);

private:
  std::vector<WorldDecorator *> decorators;
  int seed;
};

#endif
