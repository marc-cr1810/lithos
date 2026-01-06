#ifndef ORE_DECORATOR_H
#define ORE_DECORATOR_H

#include "WorldDecorator.h"
#include <filesystem>
#include <nlohmann/json.hpp>
#include <string>
#include <vector>

enum class DistributionType { UNIFORM, TRIANGLE, GAUSSIAN };

struct OreType {
  std::string code;
  std::string blockId;

  DistributionType distributionType = DistributionType::UNIFORM;
  float minY = 0.0f;
  float maxY = 1.0f;
  float peakY = 0.5f;  // For triangle distribution
  float meanY = 0.5f;  // For gaussian distribution
  float stdDev = 0.1f; // For gaussian distribution
  int veinsPerChunk = 10;

  int minVeinSize = 4;
  int maxVeinSize = 8;

  std::vector<std::string> replaceBlocks;
  std::vector<std::string> excludeBlocks;
};

class OreDecorator : public WorldDecorator {
public:
  OreDecorator();
  void LoadConfig(const std::filesystem::path &configPath);

  virtual void Decorate(Chunk &chunk, WorldGenerator &generator,
                        const struct ChunkColumn &column) override;

  virtual void Decorate(WorldGenerator &generator, WorldGenRegion &region,
                        const struct ChunkColumn &column) override;

private:
  static std::vector<OreType> oreTypes;

  float SampleDistribution(const OreType &ore, float random) const;
  bool CanReplaceBlock(const std::string &blockId, const OreType &ore) const;
  bool MatchesPattern(const std::string &pattern,
                      const std::string &blockId) const;
  void GenerateVein(WorldGenRegion &region, int x, int y, int z,
                    const OreType &ore, int size);
};

#endif
