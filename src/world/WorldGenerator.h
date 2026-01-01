#ifndef WORLDGENERATOR_H
#define WORLDGENERATOR_H

#include "Block.h"
#include "WorldGenConfig.h"
#include <FastNoise/FastNoise.h>
#include <glm/glm.hpp>
#include <map>
#include <memory>
#include <mutex>
#include <string>
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

struct ChunkColumn;

// Pre-computed noise data for batch cave generation
struct CaveNoiseData {
  static constexpr int SIZE = 32 + 4; // CHUNK_SIZE + padding for nearby samples
  float cheeseNoise[SIZE * SIZE * SIZE];
  float spaghettiMod[SIZE * SIZE * SIZE];
  float spaghettiNoise1[SIZE * SIZE * SIZE];
  float spaghettiNoise2[SIZE * SIZE * SIZE];
  float entranceNoise[SIZE * SIZE]; // 2D for surface

  // Helper to get 3D index
  inline int Index3D(int x, int y, int z) const {
    return x + y * SIZE + z * SIZE * SIZE;
  }

  // Helper to get 2D index
  inline int Index2D(int x, int z) const { return x + z * SIZE; }
};

// Landform configuration for octave-based terrain
struct LandformConfig {
  std::vector<float> octaveAmplitudes; // 8 values, 0.0-1.0
  std::vector<float> octaveThresholds; // 8 values, 0.0-1.0
  float baseHeight;                    // Base elevation (e.g., 64)
  float heightVariation;               // Max height variation (e.g., 20)
  std::string name;
};

// Cave generation system
class CaveGenerator {
public:
  CaveGenerator(const WorldGenConfig &config);

  // Check if position should be a cave (using pre-computed noise)
  bool IsCaveAt(int x, int y, int z, int maxDepth,
                const CaveNoiseData &noiseData);

  // Check if position should be a cave (single point - slow path)
  bool IsCaveAt(int x, int y, int z, int maxDepth);

  // Check if position should be a ravine
  bool IsRavineAt(int gx, int gy, int gz, int surfaceHeight);
  bool IsRavineAt(int bx, int by, int bz, int gx, int gy, int gz,
                  int surfaceHeight, const CaveNoiseData &noiseData);

  // Generate worm-style cave tunnel
  void GenerateWormCave(Chunk &chunk, int startX, int startY, int startZ,
                        int maxDepth);

  class WorldGenerator *generator; // For FastNoise3D access

private:
  void CarveSphere(Chunk &chunk, glm::vec3 center, float radius);

  int seed;
  float caveFrequency;
  float caveThreshold;
  const WorldGenConfig &config;
};

class WorldGenerator {
public:
  WorldGenerator(const WorldGenConfig &config);
  ~WorldGenerator();
  void GenerateFixedMaps(); // Pre-calculate maps if fixed world is enabled
  void GenerateColumn(ChunkColumn &column, int cx, int cz);
  void GenerateChunk(Chunk &chunk, const ChunkColumn &column);
  int GetHeight(int x, int z); // Converted to Instance Method
  float GetTemperature(int x, int z, int y = -1);
  float GetHumidity(int x, int z);
  Biome GetBiome(int x, int z, int y = -1);
  Biome GetBiomeAtHeight(int x, int z, int height, float temp = -1.0f,
                         float humid = -1.0f); // Height-aware biome
  BlockType
  GetSurfaceBlock(int gx, int gy, int gz,
                  bool checkCarving = false); // Check for subterranean features
  BlockType GetSurfaceBlock(int gx, int gy, int gz, int cachedHeight,
                            float cachedBaseTemp, float cachedHumid,
                            float cachedBeachNoise, const ChunkColumn *column,
                            bool checkCarving = false);
  float GetBeachNoise(int gx, int gz);
  bool IsCaveAt(int x, int y, int z);
  float GetCaveProbability(int x, int z);
  int GetSeed() const { return m_Seed; }
  const WorldGenConfig &GetConfig() const { return config; }
  // Returns 0.0 to 1.0 intensity of the river channel
  float GetRiverCarveFactor(int x, int z);
  float GetLandformNoise(int x, int z);
  int CalculateHeightFromNoise(float hNoise, float lNoise) const;

  // Get height for a specific landform without blending
  int GetHeightForLandform(const std::string &name, int x, int z);

  void GetLandformBlend(int x, int z, std::string &primary,
                        std::string &secondary, float &blendFactor);

  bool IsProfilingEnabled() const { return m_ProfilingEnabled; }

  void EnableProfiling(bool enable) { m_ProfilingEnabled = enable; }

  // FastNoise2 wrapper methods - PUBLIC for CaveGenerator access
  void InitializeFastNoise();
  float FastNoise2D(float x, float y, int seed = 0);
  float FastNoise3D(float x, float y, float z, int seed = 0);

  // Batch grid generation (SIMD optimized)
  void FastNoiseGrid2D(float *output, int startX, int startZ, int width,
                       int height, float frequency, int seedOffset = 0);
  void FastNoiseGrid3D(float *output, int startX, int startY, int startZ,
                       int width, int height, int depth, float frequency,
                       int seedOffset = 0);

  // Synced batch methods for decorators
  void GenerateTemperatureGrid(float *output, int startX, int startZ, int width,
                               int height) const;
  void GenerateHumidityGrid(float *output, int startX, int startZ, int width,
                            int height) const;
  void GenerateBeachGrid(float *output, int startX, int startZ, int width,
                         int height) const;
  void GenerateHeightGrid(float *output, int startX, int startZ, int width,
                          int height) const;
  void GenerateLandformGrid(float *output, int startX, int startZ, int width,
                            int height) const;

  // Generate all cave noise grids for a chunk at once (SIMD batch)
  void GenerateCaveNoiseData(CaveNoiseData &data, int chunkX, int chunkZ,
                             int chunkY);

private:
  BlockType GetStrataBlock(int x, int y, int z);
  // Compute methods (On-the-fly calculation)
  int ComputeHeight(int x, int z);
  float ComputeTemperature(int x, int z, int y = -1);
  float ComputeHumidity(int x, int z);
  Biome ComputeBiome(int x, int z, int y = -1, float preTemp = -1.0f,
                     float preHumid = -1.0f);

  // Noise map methods
  float GetClimateNoise(int x, int z);
  float GetGeologicNoise(int x, int z);
  std::string GetLandformType(int x, int z);
  int riverWaterLevel;

  // Landform initialization
  void InitializeLandforms();

  LandformConfig *GetLandform(const std::string &name);

  // Members
  std::vector<WorldDecorator *> decorators;
  std::map<std::string, LandformConfig> landforms;
  CaveGenerator *caveGenerator;
  WorldGenConfig config;
  int m_Seed;
  bool m_Initialized = false;
  bool m_ProfilingEnabled = false;
  std::mutex m_InitMutex;

  // FastNoise2 nodes
  FastNoise::SmartNode<> m_PerlinNoise2D;
  FastNoise::SmartNode<> m_PerlinNoise3D;
  FastNoise::SmartNode<> m_HeightFractal;
  FastNoise::SmartNode<> m_TemperatureNoise;
  FastNoise::SmartNode<> m_HumidityNoise;
  FastNoise::SmartNode<> m_BeachNoise;
  FastNoise::SmartNode<> m_LandformNoise;

  // Fixed world maps (Linearized 2D arrays: index = x + z * size)
  std::vector<int> fixedHeightMap;
  std::vector<float> fixedTempMap;
  std::vector<float> fixedHumidMap;
  std::vector<Biome> fixedBiomeMap;
};

#endif
