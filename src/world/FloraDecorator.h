#ifndef FLORA_DECORATOR_H
#define FLORA_DECORATOR_H

#include "WorldDecorator.h"
#include <filesystem>
#include <nlohmann/json.hpp>
#include <string>
#include <vector>

struct FloraType {
  std::string code;
  std::string blockId;
  float weight = 1.0f;

  // Spawn conditions
  float minTemp = -100.0f;
  float maxTemp = 100.0f;
  float minRain = 0.0f;
  float maxRain = 1.0f;
  float minY = 0.0f;
  float maxY = 1.0f;
  std::vector<std::string> allowedSurfaceBlocks;

  // Patch settings
  int minPatchSize = 1;
  int maxPatchSize = 4;
  float density = 0.5f;
};

class FloraDecorator : public WorldDecorator {
public:
  FloraDecorator();
  void LoadConfig(const std::filesystem::path &configPath);

  virtual void Decorate(Chunk &chunk, WorldGenerator &generator,
                        const struct ChunkColumn &column) override;

  virtual void Decorate(WorldGenerator &generator, WorldGenRegion &region,
                        const struct ChunkColumn &column) override;

private:
  static std::vector<FloraType> floraTypes;
  static int meanPatchesPerChunk;
  static int patchVariance;
  static float totalWeight;

  FloraType *SelectFlora(float temp, float rain, float y);
  void PlacePatch(WorldGenRegion &region, int centerX, int centerY, int centerZ,
                  const FloraType &flora, const ChunkColumn &column);
  bool CanPlaceAt(int x, int y, int z, const FloraType &flora,
                  WorldGenRegion &region, const ChunkColumn &column);
};

#endif
