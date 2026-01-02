#ifndef WORLDGENERATOR_H
#define WORLDGENERATOR_H

#include "Block.h"
#include "WorldGenConfig.h"
#include "gen/BlockLayerConfig.h"
#include "gen/Landform.h"
#include "gen/NoiseManager.h"
#include "gen/RockStrata.h"
#include <vector>

class Chunk;
struct ChunkColumn;
class WorldDecorator;
class CaveGenerator;

class WorldGenerator {
public:
  WorldGenerator(const WorldGenConfig &config);
  ~WorldGenerator();

  // Pre-calculate maps if fixed world is enabled
  void GenerateFixedMaps();

  // Generate a Chunk Column (Noise/Landform data)
  void GenerateColumn(ChunkColumn &column, int cx, int cz);

  // Generate blocks in a Chunk
  void GenerateChunk(Chunk &chunk, const ChunkColumn &column);

  // --- Helper / Getters ---

  int GetHeight(int x, int z);
  BlockType GetSurfaceBlock(int x, int y, int z,
                            const ChunkColumn *column = nullptr);

  // Proxy helpers for decorators
  float GetTemperature(int x, int z);
  float GetHumidity(int x, int z);
  float GetForestNoise(int x, int z);
  float GetBushNoise(int x, int z);
  float GetBeachNoise(int x, int z);
  std::string GetLandformNameAt(int x, int z);

  int GetSeed() const { return m_Seed; }
  const WorldGenConfig &GetConfig() const { return config; }

  // Access to new noise system
  const NoiseManager &GetNoiseManager() const { return noiseManager; }
  BlockLayerConfig &GetBlockLayerConfig() { return blockLayerConfig; }

  // Utilities

  // Utilities
  // Calculate Height from noise (moved from private/impl)
  int CalculateHeightFromNoise(float hNoise, float lNoise) const {
    // Placeholder if decorators really used this, but they should rely on
    // column or GetHeight
    return (int)(64.0f + hNoise * 20.0f);
  }

  // Profiling support
  bool IsProfilingEnabled() const { return m_ProfilingEnabled; }
  void EnableProfiling(bool enable) { m_ProfilingEnabled = enable; }

  // River stuff if still needed by decorators
  float GetRiverCarveFactor(int x, int z) {
    return 0.0f;
  } // Disabled/Stub for now
  float GetLandformNoise(int x, int z);

  // Batch Methods needed by Decorators (forwarded to NoiseManager)
  void GenerateHeightGrid(float *output, int startX, int startZ, int width,
                          int height) const {
    noiseManager.GenUpheaval(output, startX, startZ, width, height);
    // Note: This is just upheaval, not full height. Decorators might need full
    // calculation. But TreeDecorator uses this mainly for "where is surface".
    // Ideally it should query GetHeight which calls full pipeline.
    // For now, let's leave this stub or mapping.
  }
  void GenerateTemperatureGrid(float *output, int startX, int startZ, int width,
                               int height) const {
    // Need dummy humidity output
    std::vector<float> humid(width * height);
    noiseManager.GenClimate(output, humid.data(), startX, startZ, width,
                            height);
  }
  void GenerateHumidityGrid(float *output, int startX, int startZ, int width,
                            int height) const {
    std::vector<float> temp(width * height);
    noiseManager.GenClimate(temp.data(), output, startX, startZ, width, height);
  }
  void GenerateBeachGrid(float *output, int startX, int startZ, int width,
                         int height) const {
    noiseManager.GenBeach(output, startX, startZ, width, height);
  }

private:
  WorldGenConfig config;
  int m_Seed;
  bool m_ProfilingEnabled = false;

  // New Systems
  NoiseManager noiseManager;
  BlockLayerConfig blockLayerConfig;
  LandformRegistry &landformRegistry;
  RockStrataRegistry &strataRegistry;

  // Decorators
  std::vector<WorldDecorator *> decorators;
  CaveGenerator *caveGenerator;

  // Post-Processing
  void CleanupFloatingIslands(Chunk &chunk);
};

#endif
