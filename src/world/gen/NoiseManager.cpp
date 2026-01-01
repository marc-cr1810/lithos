#include "NoiseManager.h"
#include <iostream>

NoiseManager::NoiseManager(const WorldGenConfig &config)
    : config(config), seed(config.seed) {
  Initialize();
}

void NoiseManager::Initialize() {
  // 1. Upheaval: Massive scale, influences base height consistency
  auto upheaval = FastNoise::New<FastNoise::Simplex>();
  auto upheavalFractal = FastNoise::New<FastNoise::FractalFBm>();
  upheavalFractal->SetSource(upheaval);
  upheavalFractal->SetOctaveCount(2);
  upheavalFractal->SetGain(0.5f);
  upheavalFractal->SetLacunarity(2.0f);
  upheavalNode = upheavalFractal;

  // 2. Landform: Cellular/Voronoi for distinct regions
  // This defines distinct cells like "Mountain", "Plain"
  auto landformSource = FastNoise::New<FastNoise::CellularValue>();
  landformSource->SetDistanceFunction(FastNoise::DistanceFunction::Euclidean);

  auto domainWarp = FastNoise::New<FastNoise::DomainWarpGradient>();
  domainWarp->SetSource(landformSource);
  domainWarp->SetWarpAmplitude(20.0f);
  domainWarp->SetWarpFrequency(config.landformScale * 2.0f);
  landformNode = domainWarp;

  // New: Terrain Detail (High frequency FRACTAL for height variance)
  // This drives the actual height spline within the landform cell
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

  // 6. Beach Noise
  auto beach = FastNoise::New<FastNoise::Simplex>();
  auto beachFractal = FastNoise::New<FastNoise::FractalFBm>();
  beachFractal->SetSource(beach);
  beachFractal->SetOctaveCount(3);
  beachNode = beachFractal;
}

// --------------------------------------------------------
// Getters (Single Point)
// --------------------------------------------------------

float NoiseManager::GetUpheaval(int x, int z) const {
  // GenSingle2D returns value, usually -1 to 1 depending on source
  return upheavalNode->GenSingle2D(x * config.upheavalScale,
                                   z * config.upheavalScale, seed);
}

float NoiseManager::GetLandformNoise(int x, int z) const {
  return landformNode->GenSingle2D(x * config.landformScale,
                                   z * config.landformScale, seed + 1);
}

float NoiseManager::GetGeologicNoise(int x, int z) const {
  return geologicNode->GenSingle2D(x * config.geologicScale,
                                   z * config.geologicScale, seed + 2);
}

float NoiseManager::GetTemperature(int x, int z) const {
  return tempNode->GenSingle2D(x * config.tempScale, z * config.tempScale,
                               seed + 3);
}

float NoiseManager::GetHumidity(int x, int z) const {
  return humidNode->GenSingle2D(x * config.humidityScale,
                                z * config.humidityScale, seed + 4);
}

float NoiseManager::GetForestNoise(int x, int z) const {
  // Forest scale should be relatively high locally but forests are large
  return forestNode->GenSingle2D(x * config.forestScale, z * config.forestScale,
                                 seed + 5);
}

float NoiseManager::GetBushNoise(int x, int z) const {
  return bushNode->GenSingle2D(x * config.bushScale, z * config.bushScale,
                               seed + 6);
}

float NoiseManager::GetBeachNoise(int x, int z) const {
  return beachNode->GenSingle2D(x * config.beachScale, z * config.beachScale,
                                seed + 7);
}

// New: Terrain Detail for driving height splines
float NoiseManager::GetTerrainDetail(int x, int z) const {
  return terrainDetailNode->GenSingle2D(x * config.landformScale * 2.0f,
                                        z * config.landformScale * 2.0f,
                                        seed + 8);
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
                                 config.landformScale, seed + 1);
}

void NoiseManager::GenGeologic(float *output, int startX, int startZ, int width,
                               int height) const {
  geologicNode->GenUniformGrid2D(output, startX, startZ, width, height,
                                 config.geologicScale, seed + 2);
}

void NoiseManager::GenClimate(float *tempOut, float *humidOut, int startX,
                              int startZ, int width, int height) const {
  tempNode->GenUniformGrid2D(tempOut, startX, startZ, width, height,
                             config.tempScale, seed + 3);
  humidNode->GenUniformGrid2D(humidOut, startX, startZ, width, height,
                              config.humidityScale, seed + 4);
}

void NoiseManager::GenVegetation(float *forestOut, float *bushOut, int startX,
                                 int startZ, int width, int height) const {
  forestNode->GenUniformGrid2D(forestOut, startX, startZ, width, height,
                               config.forestScale, seed + 5);
  bushNode->GenUniformGrid2D(bushOut, startX, startZ, width, height,
                             config.bushScale, seed + 6);
}

void NoiseManager::GenBeach(float *output, int startX, int startZ, int width,
                            int height) const {
  beachNode->GenUniformGrid2D(output, startX, startZ, width, height,
                              config.beachScale, seed + 7);
}

void NoiseManager::GenTerrainDetail(float *output, int startX, int startZ,
                                    int width, int height) const {
  // Detail uses higher frequency than base landforms
  terrainDetailNode->GenUniformGrid2D(output, startX, startZ, width, height,
                                      config.landformScale * 4.0f, seed + 8);
}
