#include "NoiseManager.h"
#include <iostream>

NoiseManager::NoiseManager(const WorldGenConfig &config)
    : config(config), seed(config.seed) {
  Initialize();
}

void NoiseManager::Initialize() {
  // Helper to apply the SHARED landform warp chain
  auto applyLandformWarp = [&](auto source) {
    auto detail = FastNoise::New<FastNoise::DomainWarpGradient>();
    detail->SetSource(source);
    detail->SetWarpAmplitude(0.2f);
    detail->SetWarpFrequency(2.0f); // 2.0 is the sweet spot

    auto large = FastNoise::New<FastNoise::DomainWarpGradient>();
    large->SetSource(detail);
    large->SetWarpAmplitude(1.5f);
    large->SetWarpFrequency(0.5f);
    return large;
  };

  // 1. Upheaval: Massive scale, influences base height consistency
  auto upheaval = FastNoise::New<FastNoise::Simplex>();
  auto upheavalFractal = FastNoise::New<FastNoise::FractalFBm>();
  upheavalFractal->SetSource(upheaval);
  upheavalFractal->SetOctaveCount(2);
  upheavalFractal->SetGain(0.5f);
  upheavalFractal->SetLacunarity(2.0f);
  upheavalNode = upheavalFractal;

  // 2. Landform: Cellular/Voronoi for distinct regions
  // 2a. Landform Noise (Cell ID)
  auto landformSource = FastNoise::New<FastNoise::CellularValue>();
  landformSource->SetDistanceFunction(
      FastNoise::DistanceFunction::EuclideanSquared);
  landformSource->SetJitterModifier(1.0f);
  landformNode = applyLandformWarp(landformSource);

  // 2b. Landform Edge (F2 - F1)
  auto edgeSource = FastNoise::New<FastNoise::CellularDistance>();
  edgeSource->SetDistanceFunction(
      FastNoise::DistanceFunction::EuclideanSquared);
  edgeSource->SetReturnType(
      FastNoise::CellularDistance::ReturnType::Index0Sub1);
  edgeSource->SetJitterModifier(1.0f);
  landformEdgeNode = applyLandformWarp(edgeSource);

  // 2c. Landform Neighbor (2nd Closest Cell Value)
  auto neighborSource = FastNoise::New<FastNoise::CellularValue>();
  neighborSource->SetDistanceFunction(
      FastNoise::DistanceFunction::EuclideanSquared);
  neighborSource->SetValueIndex(1); // Get 2nd closest
  neighborSource->SetJitterModifier(1.0f);
  landformNodeNeighbor = applyLandformWarp(neighborSource);

  // 2d. Landform Neighbor 3 (3rd Closest Cell Value)
  auto neighbor3Source = FastNoise::New<FastNoise::CellularValue>();
  neighbor3Source->SetDistanceFunction(
      FastNoise::DistanceFunction::EuclideanSquared);
  neighbor3Source->SetValueIndex(2); // Get 3rd closest
  neighbor3Source->SetJitterModifier(1.0f);
  landformNodeNeighbor3 = applyLandformWarp(neighbor3Source);

  // 2e. Landform Distances (F1, F2, F3)
  auto f1Source = FastNoise::New<FastNoise::CellularDistance>();
  f1Source->SetDistanceFunction(FastNoise::DistanceFunction::EuclideanSquared);
  f1Source->SetDistanceIndex0(0);
  f1Source->SetReturnType(FastNoise::CellularDistance::ReturnType::Index0);
  f1Source->SetJitterModifier(1.0f);
  landformF1Node = applyLandformWarp(f1Source);

  auto f2Source = FastNoise::New<FastNoise::CellularDistance>();
  f2Source->SetDistanceFunction(FastNoise::DistanceFunction::EuclideanSquared);
  f2Source->SetDistanceIndex0(1);
  f2Source->SetReturnType(FastNoise::CellularDistance::ReturnType::Index0);
  f2Source->SetJitterModifier(1.0f);
  landformF2Node = applyLandformWarp(f2Source);

  auto f3Source = FastNoise::New<FastNoise::CellularDistance>();
  f3Source->SetDistanceFunction(FastNoise::DistanceFunction::EuclideanSquared);
  f3Source->SetDistanceIndex0(2);
  f3Source->SetReturnType(FastNoise::CellularDistance::ReturnType::Index0);
  f3Source->SetJitterModifier(1.0f);
  landformF3Node = applyLandformWarp(f3Source);

  // New: Terrain Detail (High frequency FRACTAL for height variance)
  auto detailSource = FastNoise::New<FastNoise::Simplex>();
  auto detailFractal = FastNoise::New<FastNoise::FractalFBm>();
  detailFractal->SetSource(detailSource);
  detailFractal->SetOctaveCount(4);
  detailFractal->SetGain(0.5f);
  detailFractal->SetLacunarity(2.0f);
  terrainDetailNode = detailFractal;

  // 3. Geologic Province
  auto geologic = FastNoise::New<FastNoise::Simplex>();
  auto geologicFractal = FastNoise::New<FastNoise::FractalFBm>();
  geologicFractal->SetSource(geologic);
  geologicFractal->SetOctaveCount(3);
  geologicNode = geologicFractal;

  // 4. Climate
  auto temp = FastNoise::New<FastNoise::Perlin>();
  auto tempFractal = FastNoise::New<FastNoise::FractalFBm>();
  tempFractal->SetSource(temp);
  tempFractal->SetOctaveCount(3);
  tempNode = tempFractal;

  auto humid = FastNoise::New<FastNoise::Perlin>();
  auto humidFractal = FastNoise::New<FastNoise::FractalFBm>();
  humidFractal->SetSource(humid);
  humidFractal->SetOctaveCount(3);
  humidNode = humidFractal;

  // 5. Vegetation
  auto forest = FastNoise::New<FastNoise::Simplex>();
  auto forestFractal = FastNoise::New<FastNoise::FractalFBm>();
  forestFractal->SetSource(forest);
  forestFractal->SetOctaveCount(4);
  forestNode = forestFractal;

  auto bushFractal = FastNoise::New<FastNoise::FractalFBm>();
  bushFractal->SetSource(forest); // Use same base
  bushFractal->SetOctaveCount(4);
  bushNode = bushFractal;

  auto beach = FastNoise::New<FastNoise::Simplex>();
  auto beachFractal = FastNoise::New<FastNoise::FractalFBm>();
  beachFractal->SetSource(beach);
  beachFractal->SetOctaveCount(3);
  beachNode = beachFractal;

  // 8. Continentalness Removed

  // 6. Strata (Smoother layers)
  auto strataSource = FastNoise::New<FastNoise::Simplex>();
  auto strataFractal = FastNoise::New<FastNoise::FractalFBm>();
  strataFractal->SetSource(strataSource);
  strataFractal->SetOctaveCount(2);
  strataFractal->SetGain(0.5f);
  strataFractal->SetLacunarity(2.0f);
  strataNode = strataFractal;

  // 7. Cave Noise (3D Cheese)
  auto caveSimplex = FastNoise::New<FastNoise::Simplex>();
  auto caveFractal = FastNoise::New<FastNoise::FractalFBm>();
  caveFractal->SetSource(caveSimplex);
  caveFractal->SetOctaveCount(3);
  cave3DNode = caveFractal;

  // 8. Cave Entrance Noise (2D)
  auto caveEntrance = FastNoise::New<FastNoise::Perlin>();
  caveEntranceNode = caveEntrance;
}

// --------------------------------------------------------
// Single Point Accessors
// --------------------------------------------------------

float NoiseManager::GetUpheaval(int x, int z) const {
  // GenSingle2D returns value, usually -1 to 1 depending on source
  return upheavalNode->GenSingle2D((float)x, (float)z, seed);
}

float NoiseManager::GetLandformNoise(int x, int z) const {
  return landformNode->GenSingle2D((float)x * config.landformScale,
                                   (float)z * config.landformScale, seed);
}

float NoiseManager::GetLandformEdgeNoise(int x, int z) const {
  return landformEdgeNode->GenSingle2D((float)x * config.landformScale,
                                       (float)z * config.landformScale, seed);
}

float NoiseManager::GetLandformNeighborNoise(int x, int z) const {
  return landformNodeNeighbor->GenSingle2D(
      (float)x * config.landformScale, (float)z * config.landformScale, seed);
}

float NoiseManager::GetGeologicNoise(int x, int z) const {
  return geologicNode->GenSingle2D((float)x * config.geologicScale,
                                   (float)z * config.geologicScale, seed);
}

float NoiseManager::GetTemperature(int x, int z) const {
  float val = tempNode->GenSingle2D((float)x * config.climateScale,
                                    (float)z * config.climateScale, seed + 1);
  // Map [-1, 1] to [-30, 60] (typical biome ranges)
  return (val + 1.0f) * 0.5f * 90.0f - 30.0f;
}

float NoiseManager::GetHumidity(int x, int z) const {
  return humidNode->GenSingle2D((float)x * config.climateScale,
                                (float)z * config.climateScale, seed + 2);
}

float NoiseManager::GetForestNoise(int x, int z) const {
  return forestNode->GenSingle2D((float)x * config.forestScale,
                                 (float)z * config.forestScale, seed + 3);
}

float NoiseManager::GetBushNoise(int x, int z) const {
  return bushNode->GenSingle2D((float)x * config.bushScale,
                               (float)z * config.bushScale, seed + 4);
}

float NoiseManager::GetBeachNoise(int x, int z) const {
  return beachNode->GenSingle2D((float)x, (float)z, seed + 5);
}

// New: Terrain Detail for driving height splines
float NoiseManager::GetTerrainDetail(int x, int z) const {
  return terrainDetailNode->GenSingle2D((float)x, (float)z, seed + 10);
}

float NoiseManager::GetLandformNeighbor3Noise(int x, int z) const {
  return landformNodeNeighbor3->GenSingle2D(
      (float)x * config.landformScale, (float)z * config.landformScale, seed);
}

void NoiseManager::GetLandformDistances(int x, int z, float &f1, float &f2,
                                        float &f3) const {
  float X = (float)x * config.landformScale;
  float Z = (float)z * config.landformScale;
  f1 = landformF1Node->GenSingle2D(X, Z, seed);
  f2 = landformF2Node->GenSingle2D(X, Z, seed);
  f3 = landformF3Node->GenSingle2D(X, Z, seed);
}

// Fixed GetCave3D to manually apply frequency scaling
float NoiseManager::GetCave3D(int x, int y, int z, float frequency) const {
  return cave3DNode->GenSingle3D((float)x * frequency, (float)y * frequency,
                                 (float)z * frequency, seed + 8);
}

// Fixed GetCaveEntrance to manually apply frequency
float NoiseManager::GetCaveEntrance(int x, int z) const {
  return caveEntranceNode->GenSingle2D((float)x * 0.012f, (float)z * 0.012f,
                                       seed + 9);
}

// --------------------------------------------------------
// Batch Generators
// --------------------------------------------------------

void NoiseManager::GenUpheaval(float *output, int startX, int startZ, int width,
                               int height) const {
  upheavalNode->GenUniformGrid2D(output, startX, startZ, width, height,
                                 config.upheavalScale, seed);
}

void NoiseManager::GenLandform(float *output, int startX, int startZ, int width,
                               int height) const {
  landformNode->GenUniformGrid2D(output, startX, startZ, width, height,
                                 config.landformScale, seed);
}

void NoiseManager::GenLandformNeighbor(float *output, int startX, int startZ,
                                       int width, int height) const {
  landformNodeNeighbor->GenUniformGrid2D(output, startX, startZ, width, height,
                                         config.landformScale, seed);
}

void NoiseManager::GenGeologic(float *output, int startX, int startZ, int width,
                               int height) const {
  geologicNode->GenUniformGrid2D(output, startX, startZ, width, height,
                                 config.geologicScale, seed);
}

void NoiseManager::GenClimate(float *tempOut, float *humidOut, int startX,
                              int startZ, int width, int height) const {
  tempNode->GenUniformGrid2D(tempOut, startX, startZ, width, height,
                             config.climateScale, seed + 1);
  humidNode->GenUniformGrid2D(humidOut, startX, startZ, width, height,
                              config.climateScale, seed + 2);

  // Scale post-generation
  for (int i = 0; i < width * height; ++i) {
    // Temp: Map [-1, 1] to [-30, 60]
    tempOut[i] = (tempOut[i] + 1.0f) * 0.5f * 90.0f - 30.0f;
    // Humidity: Map [-1, 1] to [-1, 1] (already there, but for explicitness)
    // humidOut[i] = humidOut[i];
  }
}

// Fixed GenVegetation to use correct scales
void NoiseManager::GenVegetation(float *forestOut, float *bushOut, int startX,
                                 int startZ, int width, int height) const {
  forestNode->GenUniformGrid2D(forestOut, startX, startZ, width, height,
                               config.forestScale, seed + 3);
  bushNode->GenUniformGrid2D(bushOut, startX, startZ, width, height,
                             config.bushScale, seed + 4);
}

void NoiseManager::GenBeach(float *output, int startX, int startZ, int width,
                            int height) const {
  beachNode->GenUniformGrid2D(output, startX, startZ, width, height,
                              config.beachScale, seed + 5);
}

void NoiseManager::GenTerrainDetail(float *output, int startX, int startZ,
                                    int width, int height) const {
  // Detail uses its own configurable scale (default is landformScale * 4)
  terrainDetailNode->GenUniformGrid2D(output, startX, startZ, width, height,
                                      config.terrainDetailScale, seed + 10);
}

void NoiseManager::GenLandformNeighbor3(float *output, int startX, int startZ,
                                        int width, int height) const {
  landformNodeNeighbor3->GenUniformGrid2D(output, startX, startZ, width, height,
                                          config.landformScale, seed);
}

void NoiseManager::GenLandformDistances(float *f1, float *f2, float *f3,
                                        int startX, int startZ, int width,
                                        int height) const {
  landformF1Node->GenUniformGrid2D(f1, startX, startZ, width, height,
                                   config.landformScale, seed);
  landformF2Node->GenUniformGrid2D(f2, startX, startZ, width, height,
                                   config.landformScale, seed);
  landformF3Node->GenUniformGrid2D(f3, startX, startZ, width, height,
                                   config.landformScale, seed);
}

void NoiseManager::GenStrata(float *output, int startX, int startZ, int width,
                             int height) const {
  // Low frequency for smooth strata (0.005)
  strataNode->GenUniformGrid2D(output, startX, startZ, width, height, 0.005f,
                               seed + 12);
}

void NoiseManager::GenLandformEdge(float *output, int startX, int startZ,
                                   int width, int height) const {
  landformEdgeNode->GenUniformGrid2D(output, startX, startZ, width, height,
                                     config.landformScale, seed);
}

void NoiseManager::GenCave3D(float *output, int startX, int startY, int startZ,
                             int width, int height, int depth,
                             float frequency) const {
  // Use the provided frequency (calculated from caveFrequency)
  // seed default is fine, or we can offset it if we want distinct cheese vs
  // spaghetti but CaveGenerator used the same seed logic.
  cave3DNode->GenUniformGrid3D(output, startX, startY, startZ, width, height,
                               depth, frequency, seed);
}

void NoiseManager::GenCaveEntrance(float *output, int startX, int startZ,
                                   int width, int height) const {
  // Scale was 0.012 in IsCaveAt
  caveEntranceNode->GenUniformGrid2D(output, startX, startZ, width, height,
                                     0.012f, seed);
}

void NoiseManager::GetPreview(NoiseType type, float *output, int width,
                              int height, int centerX, int centerZ) const {
  int startX = centerX - width / 2;
  int startZ = centerZ - height / 2;

  // Generate at the requested world size
  std::vector<float> tempData(width * height);

  switch (type) {
  case NoiseType::Upheaval:
    GenUpheaval(tempData.data(), startX, startZ, width, height);
    break;
  case NoiseType::Landform:
    GenLandform(tempData.data(), startX, startZ, width, height);
    break;
  case NoiseType::LandformEdge:
    GenLandformEdge(output, centerX, centerZ, width, height);
    break;
  case NoiseType::Geologic:
    GenGeologic(output, centerX, centerZ, width, height);
    break;
  case NoiseType::Temperature:
    // Only temp needed
    tempNode->GenUniformGrid2D(output, centerX, centerZ, width, height,
                               config.climateScale, seed + 1);
    for (int i = 0; i < width * height; ++i) {
      output[i] = (output[i] + 1.0f) * 0.5f * 90.0f - 30.0f;
    }
    break;
  case NoiseType::Humidity:
    // Only humid needed
    humidNode->GenUniformGrid2D(output, centerX, centerZ, width, height,
                                config.climateScale, seed + 2);
    break;
  case NoiseType::LandformNeighbor:
    GenLandformNeighbor(output, centerX, centerZ, width, height);
    break;
  case NoiseType::TerrainDetail:
    GenTerrainDetail(tempData.data(), startX, startZ, width, height);
    break;
  case NoiseType::Forest:
  case NoiseType::Bush: {
    std::vector<float> forest(width * height);
    std::vector<float> bush(width * height);
    GenVegetation(forest.data(), bush.data(), startX, startZ, width, height);
    if (type == NoiseType::Forest) {
      std::copy(forest.begin(), forest.end(), tempData.begin());
    } else {
      std::copy(bush.begin(), bush.end(), tempData.begin());
    }
    break;
  }
  case NoiseType::Beach:
    GenBeach(tempData.data(), startX, startZ, width, height);
    break;
  case NoiseType::Strata:
    GenStrata(tempData.data(), startX, startZ, width, height);
    break;
  case NoiseType::LandformNeighbor3:
    GenLandformNeighbor3(output, startX, startZ, width, height);
    break;
  }

  // Resample to 256x256 if needed
  if (width == 256 && height == 256) {
    std::copy(tempData.begin(), tempData.end(), output);
  } else {
    // Nearest neighbor resampling to 256x256
    for (int y = 0; y < 256; ++y) {
      for (int x = 0; x < 256; ++x) {
        int srcX = (x * width) / 256;
        int srcY = (y * height) / 256;
        output[y * 256 + x] = tempData[srcY * width + srcX];
      }
    }
  }
}
