#pragma once

#include "../WorldGenConfig.h"
#include <FastNoise/FastNoise.h>
#include <vector>

// Wrapper for FastNoise2 to handle multiple noise maps
class NoiseManager {
public:
  explicit NoiseManager(const WorldGenConfig &config);
  ~NoiseManager() = default;

  // Prevent copying/moving to avoid SmartNode reference issues
  NoiseManager(const NoiseManager &) = delete;
  NoiseManager &operator=(const NoiseManager &) = delete;
  NoiseManager(NoiseManager &&) = delete;
  NoiseManager &operator=(NoiseManager &&) = delete;

  void Initialize();

  // Single point sampling (slower, useful for sparse checks)
  float GetUpheaval(int x, int z) const;
  float GetLandformNoise(int x, int z) const;
  float GetLandformEdgeNoise(int x, int z) const; // New
  float GetGeologicNoise(int x, int z) const;
  float GetTemperature(int x, int z) const;
  float GetHumidity(int x, int z) const;
  float GetForestNoise(int x, int z) const;
  float GetBushNoise(int x, int z) const; // Restored
  float GetBeachNoise(int x, int z) const;
  float GetTerrainDetail(int x, int z) const;
  float GetLandformNeighborNoise(int x, int z) const; // New
  float GetStrata(int x, int z) const;
  float GetCave3D(int x, int y, int z, float frequency) const;
  float GetCaveEntrance(int x, int z) const;

  // Batch generation (SIMD optimized, preferred for chunks)
  // output must be size width * height
  void GenUpheaval(float *output, int startX, int startZ, int width,
                   int height) const;
  void GenLandformNeighbor(float *output, int startX, int startZ, int width,
                           int height) const; // New: 2nd closest biome ID
  void GenLandform(float *output, int startX, int startZ, int width,
                   int height) const;
  void GenLandformEdge(float *output, int startX, int startZ, int width,
                       int height) const; // New
  void GenGeologic(float *output, int startX, int startZ, int width,
                   int height) const;
  void GenClimate(float *tempOut, float *humidOut, int startX, int startZ,
                  int width, int height) const;
  void GenVegetation(float *forestOut, float *bushOut, int startX, int startZ,
                     int width, int height) const;
  void GenBeach(float *output, int startX, int startZ, int width,
                int height) const;
  void GenTerrainDetail(float *output, int startX, int startZ, int width,
                        int height) const; // New
  void GenStrata(float *output, int startX, int startZ, int width,
                 int height) const; // New: Smoother strata layers

  // Cave Generation (3D)
  void GenCave3D(float *output, int startX, int startY, int startZ, int width,
                 int height, int depth, float frequency) const;
  void GenCaveEntrance(float *output, int startX, int startZ, int width,
                       int height) const;

  // Preview generation for UI (centered on a point)
  enum class NoiseType {
    Upheaval,
    Landform,
    LandformEdge,
    Geologic,
    Temperature,
    Humidity,
    Erosion,
    Vegetation,
    Forest, // Restored because used in cpp switch
    Bush,   // Restored
    Beach,
    TerrainDetail,
    Strata,          // New
    LandformNeighbor // Added
  };

  void GetPreview(NoiseType type, float *output, int width, int height,
                  int centerX = 0, int centerZ = 0) const;

private:
  WorldGenConfig config;
  int seed;

  // SmartNodes for noise graph
  FastNoise::SmartNode<> upheavalNode;
  FastNoise::SmartNode<> landformNode;
  FastNoise::SmartNode<> landformEdgeNode;
  FastNoise::SmartNode<> landformNodeNeighbor; // New (Index 1)
  FastNoise::SmartNode<> geologicNode;
  FastNoise::SmartNode<> tempNode;
  FastNoise::SmartNode<> humidNode;
  FastNoise::SmartNode<> forestNode;
  FastNoise::SmartNode<> bushNode;
  FastNoise::SmartNode<> beachNode;
  FastNoise::SmartNode<> terrainDetailNode;
  FastNoise::SmartNode<> strataNode; // New
  FastNoise::SmartNode<> cave3DNode;
  FastNoise::SmartNode<> caveEntranceNode;
};
