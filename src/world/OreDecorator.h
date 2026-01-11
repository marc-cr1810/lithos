#ifndef ORE_DECORATOR_H
#define ORE_DECORATOR_H

#include "Block.h"
#include "WorldDecorator.h"
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
  int size = 0;
  int minHeight = 0;
  int maxHeight = 128;
  int density = 0;       // Number of veins per chunk
  int veinsPerChunk = 0; // Backwards compatibility / alias

  int minVeinSize = 4;
  int maxVeinSize = 8;

  std::vector<std::string> replaceBlocks;
  std::vector<std::string> excludeBlocks;

  // Runtime Optimized IDs
  block_id resolvedBlockId = 0;
  std::vector<uint8_t> resolvedReplaceBlocks; // Lookup table for fast checks
  std::vector<uint8_t> resolvedExcludeBlocks; // Lookup table for fast checks

  OreType() {
    resolvedReplaceBlocks.resize(65536, false);
    resolvedExcludeBlocks.resize(65536, false);
  }
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
  bool CanReplaceBlock(block_id blockId, const OreType &ore) const;
  bool MatchesPattern(const std::string &pattern,
                      const std::string &blockId) const;
  void GenerateDisc(WorldGenRegion &region, int chunkX, int chunkZ, int cx,
                    int cy, int cz, const OreType &ore, float radius,
                    float thickness);
};

#endif
