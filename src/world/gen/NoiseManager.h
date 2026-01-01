#pragma once

#include "../WorldGenConfig.h"
#include <FastNoise/FastNoise.h>
#include <vector>

// Wrapper for FastNoise2 to handle multiple noise maps
class NoiseManager {
public:
  explicit NoiseManager(const WorldGenConfig &config);
  ~NoiseManager() = default;

  void Initialize();

  // Single point sampling (slower, useful for sparse checks)
  float GetUpheaval(int x, int z) const;
  float GetLandformNoise(int x, int z) const;
  float GetGeologicNoise(int x, int z) const;
  float GetTemperature(int x, int z) const;
  float GetHumidity(int x, int z) const;
  float GetForestNoise(int x, int z) const;
  float GetBushNoise(int x, int z) const;
  float GetBeachNoise(int x, int z) const;
  float GetTerrainDetail(int x, int z) const; // New

  // Batch generation (SIMD optimized, preferred for chunks)
  // output must be size width * height
  void GenUpheaval(float *output, int startX, int startZ, int width,
                   int height) const;
  void GenLandform(float *output, int startX, int startZ, int width,
                   int height) const;
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

private:
  WorldGenConfig config;
  int seed;

  // FastNoise Nodes
  FastNoise::SmartNode<> upheavalNode;
  FastNoise::SmartNode<> landformNode; // Cellular/Voronoi
  FastNoise::SmartNode<> geologicNode;
  FastNoise::SmartNode<> tempNode;
  FastNoise::SmartNode<> humidNode;
  FastNoise::SmartNode<> forestNode;
  FastNoise::SmartNode<> bushNode;
  FastNoise::SmartNode<> beachNode;
  FastNoise::SmartNode<> terrainDetailNode; // New
};
