#include "NoiseManager.h"
#include <iostream>

NoiseManager::NoiseManager(const WorldGenConfig &config)
    : config(config), seed(config.seed) {
  Initialize();
}

void NoiseManager::Initialize() {
  // 0. Upheaval (Large scale height modification)
  auto upheaval = FastNoise::New<FastNoise::Simplex>();
  auto upheavalFractal = FastNoise::New<FastNoise::FractalFBm>();
  upheavalFractal->SetSource(upheaval);
  upheavalFractal->SetOctaveCount(2);
  upheavalFractal->SetGain(0.5f);
  upheavalFractal->SetLacunarity(2.0f);
  // upheavalFractal->SetFrequency(
  //     0.5f); // Use frequency logic if needed, or default // Removed
  upheavalNode = upheavalFractal;

  // 1. Setup Warp Noise (Explicit Simplex Fractal)
  // Replicating the "Large -> Detail" effect with 2 octaves or just a strong
  // fractal
  auto warpSource = FastNoise::New<FastNoise::Simplex>();

  auto warpFractal = FastNoise::New<FastNoise::FractalFBm>();
  warpFractal->SetSource(warpSource);
  warpFractal->SetOctaveCount(2); // VS uses 2 octaves for "Wobble"
  warpFractal->SetGain(0.5f);
  warpFractal->SetLacunarity(2.0f);

  // We will use this single warp node for both X and Y (with seed offset)
  // to ensure coupled but independent warping of coordinates.
  warpXNode = warpFractal;
  warpYNode = warpFractal; // Alias for clarity, same logic

  // 2. Landform: Cellular/Voronoi for distinct regions
  // UNWARPED nodes - we will feed them warped coordinates manually.

  // 2a. Landform Noise (Cell ID)
  auto landformSource = FastNoise::New<FastNoise::CellularValue>();
  landformSource->SetDistanceFunction(
      FastNoise::DistanceFunction::EuclideanSquared);
  landformSource->SetJitterModifier(1.0f);
  landformNode = landformSource;

  // 2b. Landform Edge (F2 - F1)
  // We calculate this manually from F1 and F2 dists usually, but FastNoise has
  // optimized node. HOWEVER, we need "Index0Sub1" which is Distance 1 -
  // Distance 0? FastNoise Enum: Index0Sub1 = (F1 - F0). Wait, F0 is closest. F1
  // is 2nd closest. So result is positive.
  auto edgeSource = FastNoise::New<FastNoise::CellularDistance>();
  edgeSource->SetDistanceFunction(
      FastNoise::DistanceFunction::EuclideanSquared);
  edgeSource->SetReturnType(
      FastNoise::CellularDistance::ReturnType::Index0Sub1);
  edgeSource->SetJitterModifier(1.0f);
  landformEdgeNode = edgeSource;

  // 2c. Landform Neighbor (2nd Closest Cell Value)
  auto neighborSource = FastNoise::New<FastNoise::CellularValue>();
  neighborSource->SetDistanceFunction(
      FastNoise::DistanceFunction::EuclideanSquared);
  neighborSource->SetValueIndex(1); // Get 2nd closest
  neighborSource->SetJitterModifier(1.0f);
  landformNodeNeighbor = neighborSource;

  // 2d. Landform Neighbor 3 (3rd Closest Cell Value)
  auto neighbor3Source = FastNoise::New<FastNoise::CellularValue>();
  neighbor3Source->SetDistanceFunction(
      FastNoise::DistanceFunction::EuclideanSquared);
  neighbor3Source->SetValueIndex(2); // Get 3rd closest
  neighbor3Source->SetJitterModifier(1.0f);
  landformNodeNeighbor3 = neighbor3Source;

  // 2e. Landform Distances (F1, F2, F3)
  auto f1Source = FastNoise::New<FastNoise::CellularDistance>();
  f1Source->SetDistanceFunction(FastNoise::DistanceFunction::EuclideanSquared);
  f1Source->SetDistanceIndex0(0); // Closest
  f1Source->SetReturnType(FastNoise::CellularDistance::ReturnType::Index0);
  f1Source->SetJitterModifier(1.0f);
  landformF1Node = f1Source;

  auto f2Source = FastNoise::New<FastNoise::CellularDistance>();
  f2Source->SetDistanceFunction(FastNoise::DistanceFunction::EuclideanSquared);
  f2Source->SetDistanceIndex0(1); // 2nd Closest
  f2Source->SetReturnType(FastNoise::CellularDistance::ReturnType::Index0);
  f2Source->SetJitterModifier(1.0f);
  landformF2Node = f2Source;

  auto f3Source = FastNoise::New<FastNoise::CellularDistance>();
  f3Source->SetDistanceFunction(FastNoise::DistanceFunction::EuclideanSquared);
  f3Source->SetDistanceIndex0(2); // 3rd Closest
  f3Source->SetReturnType(FastNoise::CellularDistance::ReturnType::Index0);
  f3Source->SetJitterModifier(1.0f);
  landformF3Node = f3Source;

  // ... (Rest of init)

  // New: Terrain Detail (High frequency FRACTAL for height variance)
  auto detailSource = FastNoise::New<FastNoise::Simplex>();
  auto detailFractal = FastNoise::New<FastNoise::FractalFBm>();
  detailFractal->SetSource(detailSource);
  detailFractal->SetOctaveCount(4);
  detailFractal->SetGain(0.5f);
  detailFractal->SetLacunarity(2.0f);
  terrainDetailNode = detailFractal;

  // 3. Geologic Province
  auto geologicSource = FastNoise::New<FastNoise::CellularValue>();
  geologicSource->SetDistanceFunction(FastNoise::DistanceFunction::Manhattan);
  geologicSource->SetJitterModifier(2.0f);
  geologicNode = geologicSource;

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
  bushFractal->SetSource(forest);
  bushFractal->SetOctaveCount(4);
  bushNode = bushFractal;

  auto beach = FastNoise::New<FastNoise::Simplex>();
  auto beachFractal = FastNoise::New<FastNoise::FractalFBm>();
  beachFractal->SetSource(beach);
  beachFractal->SetOctaveCount(3);
  beachNode = beachFractal;

  // 6. Strata
  auto strataSource = FastNoise::New<FastNoise::Simplex>();
  auto strataFractal = FastNoise::New<FastNoise::FractalFBm>();
  strataFractal->SetSource(strataSource);
  strataFractal->SetOctaveCount(2);
  strataFractal->SetGain(0.5f);
  strataFractal->SetLacunarity(2.0f);
  strataNode = strataFractal;

  // 7. Cave Noise
  auto caveSimplex = FastNoise::New<FastNoise::Simplex>();
  auto caveFractal = FastNoise::New<FastNoise::FractalFBm>();
  caveFractal->SetSource(caveSimplex);
  caveFractal->SetOctaveCount(3);
  cave3DNode = caveFractal;

  // 8. Cave Entrance Noise
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

// Helper for warp
// Matches GenLandformComposite
void NoiseManager::GetWarpedCoord(float x, float z, float &wx, float &wz,
                                  float scale) const {
  // Use a common warp frequency relative to the scale
  float warpFreq = 0.5f;
  float nx =
      warpXNode->GenSingle2D(x * scale * warpFreq, z * scale * warpFreq, seed);
  float nz = warpYNode->GenSingle2D(x * scale * warpFreq, z * scale * warpFreq,
                                    seed + 1337);

  float amp = 1.5f / scale; // Shift up to 1.5 "cells"
  wx = (x + nx * (1.5f / scale)) * scale;
  wz = (z + nz * (1.5f / scale)) * scale;

  // Actually, let's keep it simpler to match VS logic:
  // Warp the COORDINATE then scale it.
  float warpIntensity = 1.5f; // cell widths
  wx = (x + nx * warpIntensity / scale) * scale;
  wz = (z + nz * warpIntensity / scale) * scale;
}

float NoiseManager::GetLandformNoise(int x, int z) const {
  float wx, wz;
  GetWarpedCoord((float)x, (float)z, wx, wz, config.landformScale);
  return landformNode->GenSingle2D(wx, wz, seed);
}

float NoiseManager::GetLandformEdgeNoise(int x, int z) const {
  float wx, wz;
  GetWarpedCoord((float)x, (float)z, wx, wz, config.landformScale);
  return landformEdgeNode->GenSingle2D(wx, wz, seed);
}

float NoiseManager::GetLandformNeighborNoise(int x, int z) const {
  float wx, wz;
  GetWarpedCoord((float)x, (float)z, wx, wz, config.landformScale);
  return landformNodeNeighbor->GenSingle2D(wx, wz, seed);
}

float NoiseManager::GetGeologicNoise(int x, int z) const {
  // Geologic is unwarped (Manhattan)
  return geologicNode->GenSingle2D((float)x * config.geologicScale,
                                   (float)z * config.geologicScale, seed);
}

float NoiseManager::GetTemperature(int x, int z) const {
  float wx, wz;
  GetWarpedCoord((float)x, (float)z, wx, wz, config.climateScale);
  float val = tempNode->GenSingle2D(wx, wz, seed + 1);
  // Map [-1, 1] to [-30, 60] (typical biome ranges)
  return (val + 1.0f) * 0.5f * 90.0f - 30.0f;
}

float NoiseManager::GetHumidity(int x, int z) const {
  float wx, wz;
  GetWarpedCoord((float)x, (float)z, wx, wz, config.climateScale);
  return humidNode->GenSingle2D(wx, wz, seed + 2);
}

float NoiseManager::GetTerrainDetail(int x, int z) const {
  return terrainDetailNode->GenSingle2D((float)x * config.terrainDetailScale,
                                        (float)z * config.terrainDetailScale,
                                        seed);
}

// Samples a specific terrain octave with default frequency scaling
float NoiseManager::GetTerrainOctave(float x, float z, int octave) const {
  // Base frequency similar to VS (Wavelength ~2000 blocks for octave 0)
  float baseFreq = 0.0005f;
  float freq = baseFreq * std::pow(2.0f, (float)octave);
  // Use landformNode as a base simplex source (it's CellularValue(0), wait...)
  // Actually, I should use a Simplex node.
  // I'll use warpXNode (Simplex) or create a dedicated one in init.
  // For now, let's use warpXNode->GenSingle2D which is simplex.
  return warpXNode->GenSingle2D(x * freq, z * freq, seed + 100 + octave);
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

float NoiseManager::GetLandformNeighbor3Noise(int x, int z) const {
  float wx, wz;
  GetWarpedCoord((float)x, (float)z, wx, wz, config.landformScale);
  return landformNodeNeighbor3->GenSingle2D(wx, wz, seed);
}

void NoiseManager::GetLandformDistances(int x, int z, float &f1, float &f2,
                                        float &f3) const {
  float wx, wz;
  GetWarpedCoord((float)x, (float)z, wx, wz, config.landformScale);
  f1 = landformF1Node->GenSingle2D(wx, wz, seed);
  f2 = landformF2Node->GenSingle2D(wx, wz, seed);
  f3 = landformF3Node->GenSingle2D(wx, wz, seed);
}

float NoiseManager::GetStrata(int x, int z) const {
  return strataNode->GenSingle2D((float)x, (float)z, seed + 12);
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

void NoiseManager::GenLandformComposite(float *landformOut, float *neighborOut,
                                        float *neighbor3Out, float *f1Out,
                                        float *f2Out, float *f3Out,
                                        float *edgeOut, int startX, int startZ,
                                        int width, int height) const {
  // 1. Generate Coordinates
  std::vector<float> xPos(width * height);
  std::vector<float> yPos(width * height);

  // Generate Warp Fields (Independent Domain Warp)
  // X Warp
  warpXNode->GenUniformGrid2D(xPos.data(), startX, startZ, width, height,
                              config.landformScale, seed);
  // Y Warp (different seed)
  warpYNode->GenUniformGrid2D(yPos.data(), startX, startZ, width, height,
                              config.landformScale, seed + 1337);

  float warpAmp = 1.5f; // Matches "Large" warp
  // Apply warp to positions
  // Note: GenUniformGrid2D fills xPos with Noise values. We need to ADD them to
  // coordinate.
  for (int y = 0; y < height; ++y) {
    for (int x = 0; x < width; ++x) {
      int idx = y * width + x;
      float wx = (static_cast<float>(startX + x) * config.landformScale);
      float wz = (static_cast<float>(startZ + y) * config.landformScale);

      // xPos[idx] and yPos[idx] currently hold the NOISE value (-1..1)
      float offsetX = xPos[idx] * warpAmp;
      float offsetZ = yPos[idx] * warpAmp;

      // Update the buffer to hold the Warped Coordinate
      xPos[idx] = wx + offsetX;
      yPos[idx] = wz + offsetZ;
    }
  }

  // 2. Sample Unwarped Cellular Nodes using Warped Coordinates
  if (landformOut)
    landformNode->GenPositionArray2D(landformOut, width * height, xPos.data(),
                                     yPos.data(), 0.0f, 0.0f, seed);
  if (neighborOut)
    landformNodeNeighbor->GenPositionArray2D(neighborOut, width * height,
                                             xPos.data(), yPos.data(), 0.0f,
                                             0.0f, seed);
  if (neighbor3Out)
    landformNodeNeighbor3->GenPositionArray2D(neighbor3Out, width * height,
                                              xPos.data(), yPos.data(), 0.0f,
                                              0.0f, seed);
  if (f1Out)
    landformF1Node->GenPositionArray2D(f1Out, width * height, xPos.data(),
                                       yPos.data(), 0.0f, 0.0f, seed);
  if (f2Out)
    landformF2Node->GenPositionArray2D(f2Out, width * height, xPos.data(),
                                       yPos.data(), 0.0f, 0.0f, seed);
  if (f3Out)
    landformF3Node->GenPositionArray2D(f3Out, width * height, xPos.data(),
                                       yPos.data(), 0.0f, 0.0f, seed);
  if (edgeOut)
    landformEdgeNode->GenPositionArray2D(edgeOut, width * height, xPos.data(),
                                         yPos.data(), 0.0f, 0.0f, seed);
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
  }
}

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
  terrainDetailNode->GenUniformGrid2D(output, startX, startZ, width, height,
                                      config.terrainDetailScale, seed + 10);
}

void NoiseManager::GenStrata(float *output, int startX, int startZ, int width,
                             int height) const {
  strataNode->GenUniformGrid2D(output, startX, startZ, width, height,
                               config.strataScale, seed + 12);
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

// Preview generation for UI (centered on a point)
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
    GenLandformComposite(tempData.data(), nullptr, nullptr, nullptr, nullptr,
                         nullptr, nullptr, startX, startZ, width, height);
    break;
  case NoiseType::LandformEdge:
    GenLandformComposite(nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
                         tempData.data(), centerX, centerZ, width, height);
    break;
  case NoiseType::Geologic:
    GenGeologic(tempData.data(), centerX, centerZ, width, height);
    break;
  case NoiseType::Temperature:
    // Only temp needed
    tempNode->GenUniformGrid2D(tempData.data(), centerX, centerZ, width, height,
                               config.climateScale, seed + 1);
    for (int i = 0; i < width * height; ++i) {
      tempData[i] = (tempData[i] + 1.0f) * 0.5f * 90.0f - 30.0f;
    }
    // Copy to output handled at end
    break;
  case NoiseType::Humidity:
    // Only humid needed
    humidNode->GenUniformGrid2D(tempData.data(), centerX, centerZ, width,
                                height, config.climateScale, seed + 2);
    break;
  case NoiseType::LandformNeighbor:
    GenLandformComposite(nullptr, tempData.data(), nullptr, nullptr, nullptr,
                         nullptr, nullptr, centerX, centerZ, width, height);
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
    GenLandformComposite(nullptr, nullptr, output, nullptr, nullptr, nullptr,
                         nullptr, startX, startZ, width, height);
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

// 3D Terrain Noise
float NoiseManager::GetTerrainNoise3D(int x, int y, int z) const {
  // Use terrainDetailNode but as 3D source?
  // FastNoise2 nodes are often dimension-agnostic, but let's verify if
  // terrainDetailNode (FractalFBm) supports 3D input.
  // Standard Simplex/Fractal does.
  // We use the same 'terrainDetailNode' which is FractalFBm -> Simplex.
  // We scale coordinates by detailScale.
  return terrainDetailNode->GenSingle3D((float)x * config.terrainDetailScale,
                                        (float)y * config.terrainDetailScale,
                                        (float)z * config.terrainDetailScale,
                                        seed + 10);
}

void NoiseManager::GenTerrainNoise3D(float *output, int startX, int startY,
                                     int startZ, int width, int height,
                                     int depth) const {
  terrainDetailNode->GenUniformGrid3D(output, startX, startY, startZ, width,
                                      height, depth, config.terrainDetailScale,
                                      seed + 10);
}
