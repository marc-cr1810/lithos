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
  // Amplitude ~1.0 means warp by approx 1 cell width
  domainWarp->SetWarpAmplitude(1.0f);
  // Frequency relative to the scaled coordinates (passed in GenUniformGrid)
  // 0.5f means warp is larger/smoother than the cells
  domainWarp->SetWarpFrequency(0.5f);
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
  edgeWarp->SetWarpAmplitude(1.0f);
  edgeWarp->SetWarpFrequency(0.5f);
  landformEdgeNode = edgeWarp;

  // 2c. Landform Neighbor (2nd Closest Cell Value)
  // Used for Voronoi blending
  auto neighborSource = FastNoise::New<FastNoise::CellularValue>();
  neighborSource->SetDistanceFunction(FastNoise::DistanceFunction::Euclidean);
  neighborSource->SetValueIndex(1); // Get 2nd closest

  auto neighborWarp = FastNoise::New<FastNoise::DomainWarpGradient>();
  neighborWarp->SetSource(neighborSource);
  neighborWarp->SetWarpAmplitude(1.0f);
  neighborWarp->SetWarpFrequency(0.5f);
  landformNodeNeighbor = neighborWarp;

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
  return tempNode->GenSingle2D((float)x * config.climateScale,
                               (float)z * config.climateScale, seed + 1);
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
