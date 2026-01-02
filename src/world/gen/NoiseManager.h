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
  float GetSurfacePatchNoise(int x,
                             int z) const; // New: For gravel/sand patches
  float GetTerrainOctave(float x, float z, int octave) const; // New
  float GetLandformNeighborNoise(int x, int z) const;         // 2nd closest
  float GetLandformNeighbor3Noise(int x, int z) const;        // 3rd closest
  void GetLandformDistances(int x, int z, float &f1, float &f2,
                            float &f3) const; // Raw F1, F2, F3
  float GetStrata(int x, int z) const;
  float GetCave3D(int x, int y, int z, float frequency) const;
  float GetCaveEntrance(int x, int z) const;

  // Batch generation (SIMD optimized, preferred for chunks)
  // output must be size width * height
  void GenUpheaval(float *output, int startX, int startZ, int width,
                   int height) const;

  // Combined Landform Generator (Guarantees warp consistency)
  void GenLandformComposite(float *landformOut, float *neighborOut,
                            float *neighbor3Out, float *f1Out, float *f2Out,
                            float *f3Out, float *edgeOut, int startX,
                            int startZ, int width, int height) const;

  void GenGeologic(float *output, int startX, int startZ, int width,
                   int height) const;
  void GenClimate(float *tempOut, float *humidOut, int startX, int startZ,
                  int width, int height) const;
  void GenVegetation(float *forestOut, float *bushOut, int startX, int startZ,
                     int width, int height) const;
  void GenBeach(float *output, int startX, int startZ, int width,
                int height) const;
  void GenTerrainDetail(float *output, int startX, int startZ, int width,
                        int height) const;

  void GenStrata(float *output, int startX, int startZ, int width,
                 int height) const; // New: Smoother strata layers

  // 3D Terrain Noise Batch Generation
  void GenTerrainNoise3D(float *output, int startX, int startY, int startZ,
                         int width, int height, int depth) const;

  // Single point access for 3D terrain density
  float GetTerrainNoise3D(int x, int y, int z) const;

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
    Strata,           // New
    LandformNeighbor, // Added
    LandformNeighbor3
  };

  void GetPreview(NoiseType type, float *output, int width, int height,
                  int centerX = 0, int centerZ = 0) const;

private:
  WorldGenConfig config;
  int seed;

  // SmartNodes for noise graph
  FastNoise::SmartNode<> upheavalNode;
  // Landform: Unwarped base nodes (Warp applied manually in Composite)
  FastNoise::SmartNode<> landformNode;          // CellularValue(0)
  FastNoise::SmartNode<> landformNodeNeighbor;  // CellularValue(1)
  FastNoise::SmartNode<> landformNodeNeighbor3; // CellularValue(2)
  FastNoise::SmartNode<>
      landformEdgeNode; // CellularDistance(Index0Sub1)?? No, calc manually? Or
                        // keep node. FastNoise doesn't support Index0Sub1
                        // easily via GenPositionArray? Use Node.
  FastNoise::SmartNode<> landformF1Node; // CellularDistance(0)
  FastNoise::SmartNode<> landformF2Node; // CellularDistance(1)
  FastNoise::SmartNode<> landformF3Node; // CellularDistance(2)

  FastNoise::SmartNode<> geologicNode;
  FastNoise::SmartNode<> tempNode;
  FastNoise::SmartNode<> humidNode;
  FastNoise::SmartNode<> forestNode;
  FastNoise::SmartNode<> bushNode;
  FastNoise::SmartNode<> beachNode;
  FastNoise::SmartNode<> terrainDetailNode;
  FastNoise::SmartNode<> strataNode;

  // Warp Noise Nodes
  FastNoise::SmartNode<> warpXNode;
  FastNoise::SmartNode<> warpYNode;

  // Helper for warp consistency
  void GetWarpedCoord(float x, float z, float &wx, float &wz,
                      float scale) const;

  FastNoise::SmartNode<> cave3DNode;
  FastNoise::SmartNode<> caveEntranceNode;
  FastNoise::SmartNode<> surfacePatchNode; // Simplex
};
