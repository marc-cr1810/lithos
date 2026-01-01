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

  // 2b. Landform Edge Detection (Cellular Distance F2-F1)
  // Used to blend/flatten terrain at biome borders to prevent cliffs
  auto edgeSource = FastNoise::New<FastNoise::CellularDistance>();
  edgeSource->SetDistanceFunction(FastNoise::DistanceFunction::Euclidean);
  // We want F2 - F1.
  // Configure Index0=1 (F2), Index1=0 (F1).
  // Then Index0Sub1 = F2 - F1.
  edgeSource->SetDistanceIndex0(1);
  edgeSource->SetDistanceIndex1(0);
  edgeSource->SetReturnType(
      FastNoise::CellularDistance::ReturnType::Index0Sub1);

  // Apply SAME warp to ensure edges align with values
  auto edgeWarp = FastNoise::New<FastNoise::DomainWarpGradient>();
  edgeWarp->SetSource(edgeSource);
  edgeWarp->SetWarpAmplitude(20.0f);
  edgeWarp->SetWarpFrequency(config.landformScale * 2.0f);
  landformEdgeNode = edgeWarp;

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

  auto beach = FastNoise::New<FastNoise::Simplex>();
  auto beachFractal = FastNoise::New<FastNoise::FractalFBm>();
  beachFractal->SetSource(beach);
  beachFractal->SetOctaveCount(3);
  beachNode = beachFractal;

  // 6. Strata (Smoother layers)
  auto strataSource = FastNoise::New<FastNoise::Simplex>();
  auto strataFractal = FastNoise::New<FastNoise::FractalFBm>();
  strataFractal->SetSource(strataSource);
  strataFractal->SetOctaveCount(2);
  strataFractal->SetGain(0.5f);
  strataFractal->SetLacunarity(2.0f);
  strataNode = strataFractal;

  // 7. Cave Noise (3D Cheese)
  // Matches CaveGenerator settings: Simplex -> FractalFBm (3 octaves)
  auto caveSimplex = FastNoise::New<FastNoise::Simplex>();
  auto caveFractal = FastNoise::New<FastNoise::FractalFBm>();
  caveFractal->SetSource(caveSimplex);
  caveFractal->SetOctaveCount(3);
  cave3DNode = caveFractal;

  // 8. Cave Entrance Noise (2D)
  // Matches CaveGenerator settings: Perlin (2D)
  auto caveEntrance = FastNoise::New<FastNoise::Perlin>();
  caveEntranceNode = caveEntrance;
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
  // Detail uses its own configurable scale (default is landformScale * 4)
  terrainDetailNode->GenUniformGrid2D(output, startX, startZ, width, height,
                                      config.terrainDetailScale, seed + 8);
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
                                     config.landformScale, seed + 1);
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
    GenLandformEdge(tempData.data(), startX, startZ, width, height);
    break;
  case NoiseType::Geologic:
    GenGeologic(tempData.data(), startX, startZ, width, height);
    break;
  case NoiseType::Temperature:
  case NoiseType::Humidity: {
    std::vector<float> temp(width * height);
    std::vector<float> humid(width * height);
    GenClimate(temp.data(), humid.data(), startX, startZ, width, height);
    if (type == NoiseType::Temperature) {
      std::copy(temp.begin(), temp.end(), tempData.begin());
    } else {
      std::copy(humid.begin(), humid.end(), tempData.begin());
    }
    break;
  }
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
