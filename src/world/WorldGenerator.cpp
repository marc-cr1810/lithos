#include "WorldGenerator.h"
#include "../debug/Logger.h"
#include "../debug/Profiler.h"
// #include "BlockRegistry.h"
#include "CaveGenerator.h"
#include "Chunk.h"
#include "ChunkColumn.h"
#include "FloraDecorator.h"
#include "OreDecorator.h"
#include "TreeDecorator.h"
#include "gen/NoiseManager.h"
#include <algorithm>
#include <iostream>

WorldGenerator::WorldGenerator(const WorldGenConfig &config)
    : config(config), m_Seed(config.seed), noiseManager(config),
      landformRegistry(LandformRegistry::Get()),
      strataRegistry(RockStrataRegistry::Get()) {
  // VS Style Sea Level: ~43% of world height
  this->config.seaLevel = (int)(config.worldHeight * 0.4313725490196078);
  LOG_INFO("WorldGenerator: Calculated Sea Level: {} (World Height: {})",
           this->config.seaLevel, config.worldHeight);

  // Data assets should be loaded globally in Application::Init
  // LandformRegistry::Get().LoadFromJson("assets/worldgen/landforms.json");
  // RockStrataRegistry::Get().LoadStrataLayers("assets/worldgen/rockstrata.json");
  // RockStrataRegistry::Get().LoadProvinces("assets/worldgen/geologicprovinces.json");

  // Initialize Decorators
  caveGenerator = new CaveGenerator(this->config);
  decorators.push_back(new OreDecorator());
  decorators.push_back(new TreeDecorator());
  decorators.push_back(new FloraDecorator());

  // Decorators initialized above
}

WorldGenerator::~WorldGenerator() {
  for (auto *d : decorators) {
    delete d;
  }
  delete caveGenerator;
}

void WorldGenerator::GenerateFixedMaps() {}

void WorldGenerator::GenerateColumn(ChunkColumn &column, int cx, int cz) {
  PROFILE_SCOPE_CONDITIONAL("ChunkGen_Column", m_ProfilingEnabled);

  int startX = cx * CHUNK_SIZE;
  int startZ = cz * CHUNK_SIZE;

  // 1. Generate Noise Grids (Thread-Local Buffers)
  static thread_local std::vector<float> upheaval(CHUNK_SIZE * CHUNK_SIZE);
  static thread_local std::vector<float> landformNoise(CHUNK_SIZE * CHUNK_SIZE);
  static thread_local std::vector<float> edgeNoise(CHUNK_SIZE *
                                                   CHUNK_SIZE); // F2 - F1
  static thread_local std::vector<float> landformNeighbor(
      CHUNK_SIZE * CHUNK_SIZE); // Index 1
  static thread_local std::vector<float> landformNeighbor3(
      CHUNK_SIZE * CHUNK_SIZE); // Index 2
  static thread_local std::vector<float> landformF1(CHUNK_SIZE * CHUNK_SIZE);
  static thread_local std::vector<float> landformF2(CHUNK_SIZE * CHUNK_SIZE);
  static thread_local std::vector<float> landformF3(CHUNK_SIZE * CHUNK_SIZE);
  static thread_local std::vector<float> provinceNoise(CHUNK_SIZE * CHUNK_SIZE);
  static thread_local std::vector<float> tempMap(CHUNK_SIZE * CHUNK_SIZE);
  static thread_local std::vector<float> humidMap(CHUNK_SIZE * CHUNK_SIZE);
  static thread_local std::vector<float> terrainDetail(CHUNK_SIZE * CHUNK_SIZE);
  static thread_local std::vector<float> strataNoise(CHUNK_SIZE * CHUNK_SIZE);
  static thread_local std::vector<float> forestMap(CHUNK_SIZE * CHUNK_SIZE);
  static thread_local std::vector<float> bushMap(CHUNK_SIZE * CHUNK_SIZE);
  static thread_local std::vector<float> beachMap(CHUNK_SIZE * CHUNK_SIZE);

  {
    PROFILE_SCOPE_CONDITIONAL("ChunkGen_Noise_Upheaval", m_ProfilingEnabled);
    noiseManager.GenUpheaval(upheaval.data(), startX, startZ, CHUNK_SIZE,
                             CHUNK_SIZE);
  }

  // Combined Landform Generation (Warped & Consistent)
  {
    PROFILE_SCOPE_CONDITIONAL("ChunkGen_Noise_LandformCombined",
                              m_ProfilingEnabled);
    noiseManager.GenLandformComposite(
        landformNoise.data(), landformNeighbor.data(), landformNeighbor3.data(),
        landformF1.data(), landformF2.data(), landformF3.data(),
        edgeNoise.data(), startX, startZ, CHUNK_SIZE, CHUNK_SIZE);
  }

  {
    PROFILE_SCOPE_CONDITIONAL("ChunkGen_Noise_Geologic", m_ProfilingEnabled);
    noiseManager.GenGeologic(provinceNoise.data(), startX, startZ, CHUNK_SIZE,
                             CHUNK_SIZE);
  }
  {
    PROFILE_SCOPE_CONDITIONAL("ChunkGen_Noise_Climate", m_ProfilingEnabled);
    noiseManager.GenClimate(tempMap.data(), humidMap.data(), startX, startZ,
                            CHUNK_SIZE, CHUNK_SIZE);
  }
  {
    PROFILE_SCOPE_CONDITIONAL("ChunkGen_Noise_TerrainDetail",
                              m_ProfilingEnabled);
    noiseManager.GenTerrainDetail(terrainDetail.data(), startX, startZ,
                                  CHUNK_SIZE, CHUNK_SIZE);
  }
  {
    PROFILE_SCOPE_CONDITIONAL("ChunkGen_Noise_Strata", m_ProfilingEnabled);
    noiseManager.GenStrata(strataNoise.data(), startX, startZ, CHUNK_SIZE,
                           CHUNK_SIZE);
  }
  {
    PROFILE_SCOPE_CONDITIONAL("ChunkGen_Noise_Vegetation", m_ProfilingEnabled);
    noiseManager.GenVegetation(forestMap.data(), bushMap.data(), startX, startZ,
                               CHUNK_SIZE, CHUNK_SIZE);
  }
  {
    PROFILE_SCOPE_CONDITIONAL("ChunkGen_Noise_Beach", m_ProfilingEnabled);
    noiseManager.GenBeach(beachMap.data(), startX, startZ, CHUNK_SIZE,
                          CHUNK_SIZE);
  }

  // 2. Process Columns
  {
    PROFILE_SCOPE_CONDITIONAL("ChunkGen_Column_Processing", m_ProfilingEnabled);
    for (int index = 0; index < CHUNK_SIZE * CHUNK_SIZE; ++index) {
      int lx = index % CHUNK_SIZE;
      int lz = index / CHUNK_SIZE;
      int wx = startX + lx;
      int wz = startZ + lz;

      // Fill Column Data
      column.temperatureMap[lx][lz] = tempMap[index];
      column.humidityMap[lx][lz] = humidMap[index];
      column.forestNoiseMap[lx][lz] = forestMap[index];
      column.bushNoiseMap[lx][lz] = bushMap[index];
      column.beachNoiseMap[lx][lz] = beachMap[index];

      // --- Landform Selection ---
      // VS uses position-based seeded random, not noise values!
      // We use the world position directly for each column
      float t = tempMap[index];
      float h = humidMap[index];

      // Use world X/Z for position-seeded selection (scaled to cellular grid)
      // landformScale is frequency; wavelength = 1/frequency gives cell size
      int cellScale = std::max(1, (int)(1.0f / config.landformScale));
      int cellX = wx / cellScale;
      int cellZ = wz / cellScale;

      Landform lf1 = landformRegistry.Select(cellX, cellZ, t, h);
      // For now, use same landform for all 3 (we can add neighbor lookup later)
      Landform lf2 = lf1;
      Landform lf3 = lf1;

      float upVal = upheaval[index];
      // float baseHeight = 64.0f + upVal * 20.0f; // Upheaval handled in
      // density now? VS Upheaval shifts the threshold or the Y? VS: distY =
      // oceanicity + Compute... VS: StartSampleDisplacedYThreshold(posY + distY
      // ... So Upheaval shifts the Y coordinate used for sampling the
      // Threshold.

      float upheavalYShift = upVal * 40.0f; // Match approx amplitude

      // --- ADAPTIVE BLEND RADIUS (Identical to before) ---
      // We calculate weights first to use in density blend
      // Note: We don't have 'h1' anymore to adapt radius width...
      // Fallback to fixed radii or based on Landform ID similarity?
      // For Phase 1: Constant Radius to simplify 3D transition.
      // Or calculate specific weights from noise values directly.

      float f1 = landformF1[index];
      float f2 = landformF2[index];
      float f3 = landformF3[index];

      float R = 0.3f; // Fixed radius for now

      float w1 = 1.0f;
      float w2 = 0.0f;
      float w3 = 0.0f;

      if (f2 - f1 < R) {
        float t2 = 1.0f - (f2 - f1) / R;
        w2 = t2 * t2 * t2 * (t2 * (t2 * 6.0f - 15.0f) + 10.0f);
      }
      if (f3 - f1 < R) {
        float t3 = 1.0f - (f3 - f1) / R;
        w3 = t3 * t3 * t3 * (t3 * (t3 * 6.0f - 15.0f) + 10.0f);
      }
      float totalW = w1 + w2 + w3;
      w1 /= totalW;
      w2 /= totalW;
      w3 /= totalW;

      // --- 3D Density Search ---
      int surfaceY = 0;

      // Optimization: coarse search or just top-down
      // We search from Top down.
      // To prevent floating islands invalidating the "HeightMap" concept (which
      // assumes solid below), we might need to be careful. But for now, finding
      // the HIGHEST solid block is correct for "Sunlight" / HeightMap.

      for (int y = config.worldHeight - 1; y > 0; y--) {
        // 1. Get Landform Thresholds at shifted Y
        // We apply upheaval to Y here.
        // If Y=100 and Upheaval=20, we sample threshold at Y=120 (effectively
        // pushing terrain down? No). If Upheaval is Positive, we want Higher
        // Terrain. Threshold(Y) usually decreases with Y. (1 at bottom, -1 at
        // top). If we want mountain (Limit is higher), we should sample a LOWER
        // Y's threshold. So sampleY = y - upheaval. Example: Real Y=100.
        // Upheaval=20. Sample Y=80. Threshold(80) > Threshold(100). More likely
        // solid. Correct.

        int sampleY = (int)(y - upheavalYShift);

        float th1 = lf1.GetDensityThreshold(sampleY);
        float th2 = lf2.GetDensityThreshold(sampleY);
        float th3 = lf3.GetDensityThreshold(sampleY);

        float threshold = th1 * w1 + th2 * w2 + th3 * w3;

        // 2. Get 3D Noise (The "Wiggle")
        // We can use the 3D noise from manager.
        // Note: Calling this per block is slow-ish.
        float noise3d = noiseManager.GetTerrainNoise3D(wx, y, wz);

        // 3. Density Check
        // Density = Noise + Threshold.
        if (noise3d + threshold > 0) {
          surfaceY = y;
          break;
        }
      }

      // Clamp
      if (surfaceY < 5)
        surfaceY = 5; // Bedrock
      if (surfaceY > 255)
        surfaceY = 255;

      column.setHeight(lx, lz, surfaceY);
      // We use Biome A properties for blocks (could blend properties too but
      // complex) column.SetBiome(lfA.name); // Removed as ChunkColumn has no
      // biome map

      int waterLevel = config.seaLevel;

      float pNoise = provinceNoise[index];
      float sNoise = strataNoise[index];

      // Store surface height in column for decorators
      // (SetHeight already does this potentially, but just to be sure)
    }
  }
} // End GenerateColumn

void WorldGenerator::GenerateChunk(Chunk &chunk, const ChunkColumn &column) {
  PROFILE_SCOPE_CONDITIONAL("ChunkGen_Chunk", m_ProfilingEnabled);

  int cx = chunk.chunkPosition.x;
  int cy = chunk.chunkPosition.y;
  int cz = chunk.chunkPosition.z;

  int startX = cx * CHUNK_SIZE;
  int startY = cy * CHUNK_SIZE;
  int startZ = cz * CHUNK_SIZE;

  // Regenerate Province Noise
  static thread_local std::vector<float> provinceNoise(CHUNK_SIZE * CHUNK_SIZE);
  noiseManager.GenGeologic(provinceNoise.data(), startX, startZ, CHUNK_SIZE,
                           CHUNK_SIZE);

  // Generate Strata Noise
  static thread_local std::vector<float> strataNoise(CHUNK_SIZE * CHUNK_SIZE);
  noiseManager.GenStrata(strataNoise.data(), startX, startZ, CHUNK_SIZE,
                         CHUNK_SIZE);

  // Regenerate Landform Noise & Distances for Density Calculation
  // We need these to calculate weights per column
  static thread_local std::vector<float> landformNoise(CHUNK_SIZE * CHUNK_SIZE);
  static thread_local std::vector<float> landformNeighbor(CHUNK_SIZE *
                                                          CHUNK_SIZE);
  static thread_local std::vector<float> landformNeighbor3(CHUNK_SIZE *
                                                           CHUNK_SIZE);
  static thread_local std::vector<float> landformF1(CHUNK_SIZE * CHUNK_SIZE);
  static thread_local std::vector<float> landformF2(CHUNK_SIZE * CHUNK_SIZE);
  static thread_local std::vector<float> landformF3(CHUNK_SIZE * CHUNK_SIZE);
  static thread_local std::vector<float> upheaval(CHUNK_SIZE * CHUNK_SIZE);

  // We also need temp/humid for selection
  static thread_local std::vector<float> tempMap(CHUNK_SIZE * CHUNK_SIZE);
  static thread_local std::vector<float> humidMap(CHUNK_SIZE * CHUNK_SIZE);

  noiseManager.GenLandformComposite(
      landformNoise.data(), landformNeighbor.data(), landformNeighbor3.data(),
      landformF1.data(), landformF2.data(), landformF3.data(), nullptr, startX,
      startZ, CHUNK_SIZE, CHUNK_SIZE);

  noiseManager.GenUpheaval(upheaval.data(), startX, startZ, CHUNK_SIZE,
                           CHUNK_SIZE);
  noiseManager.GenClimate(tempMap.data(), humidMap.data(), startX, startZ,
                          CHUNK_SIZE, CHUNK_SIZE);

  {
    PROFILE_SCOPE_CONDITIONAL("ChunkGen_Terrain", m_ProfilingEnabled);

    // Cache common blocks
    Block *waterBlock = BlockRegistry::getInstance().getBlock(BlockType::WATER);
    Block *airBlock = BlockRegistry::getInstance().getBlock(BlockType::AIR);
    Block *sandBlock = BlockRegistry::getInstance().getBlock(BlockType::SAND);
    Block *sandstoneBlock =
        BlockRegistry::getInstance().getBlock(BlockType::SANDSTONE);
    Block *gravelBlock =
        BlockRegistry::getInstance().getBlock(BlockType::GRAVEL);
    Block *grassBlock = BlockRegistry::getInstance().getBlock(BlockType::GRASS);
    Block *dirtBlock = BlockRegistry::getInstance().getBlock(BlockType::DIRT);

    // Lambda to get density (Similar to GenerateColumn but for specific y)
    auto checkDensity = [&](int x, int z, int y, int idx, const Landform &lf1,
                            const Landform &lf2, const Landform &lf3, float w1,
                            float w2, float w3) -> bool {
      float upVal = upheaval[idx];
      float upheavalYShift = upVal * 40.0f;
      int sampleY = (int)(y - upheavalYShift);

      float th1 = lf1.GetDensityThreshold(sampleY);
      float th2 = lf2.GetDensityThreshold(sampleY);
      float th3 = lf3.GetDensityThreshold(sampleY);

      float threshold = th1 * w1 + th2 * w2 + th3 * w3;
      float noise3d = noiseManager.GetTerrainNoise3D(startX + x, y, startZ + z);

      return (noise3d + threshold) > 0;
    };

    for (int lx = 0; lx < CHUNK_SIZE; lx++) {
      for (int lz = 0; lz < CHUNK_SIZE; lz++) {
        int wx = startX + lx;
        int wz = startZ + lz;
        int index = lx + lz * CHUNK_SIZE;

        // Pre-calculate weights for this column
        float f1 = landformF1[index];
        float f2 = landformF2[index];
        float f3 = landformF3[index];

        float R = 0.3f;
        float w1 = 1.0f;
        float w2 = 0.0f;
        float w3 = 0.0f;
        if (f2 - f1 < R) {
          float t2 = 1.0f - (f2 - f1) / R;
          w2 = t2 * t2 * t2 * (t2 * (t2 * 6.0f - 15.0f) + 10.0f);
        }
        if (f3 - f1 < R) {
          float t3 = 1.0f - (f3 - f1) / R;
          w3 = t3 * t3 * t3 * (t3 * (t3 * 6.0f - 15.0f) + 10.0f);
        }
        float totalW = w1 + w2 + w3;
        w1 /= totalW;
        w2 /= totalW;
        w3 /= totalW;

        // Select Landforms using world position
        int cellScale = std::max(1, (int)(1.0f / config.landformScale));
        int cellX = wx / cellScale;
        int cellZ = wz / cellScale;

        Landform lf1 = landformRegistry.Select(cellX, cellZ, tempMap[index],
                                               humidMap[index]);
        Landform lf2 = lf1; // Use same for all 3
        Landform lf3 = lf1;

        int surfaceHeight = column.getHeight(lx, lz);
        float pNoise = provinceNoise[index];
        float sNoise = strataNoise[index];

        for (int ly = 0; ly < CHUNK_SIZE; ly++) {
          int wy = startY + ly;

          bool isSolid = false;

          if (wy <= surfaceHeight) {
            // Below the calculated "surface", checks density
            // Optimization: Deep underground is always solid?
            // Only check density if within some range of surface?
            // For full overhang support, we must check.
            isSolid =
                checkDensity(lx, lz, wy, index, lf1, lf2, lf3, w1, w2, w3);
          }

          // 1. Base Terrain
          if (isSolid) {
            BlockType rockType =
                strataRegistry.GetStrataBlock(wx, wy, wz, surfaceHeight, pNoise,
                                              sNoise, upheaval[index], m_Seed);
            chunk.blocks[lx][ly][lz].block =
                BlockRegistry::getInstance().getBlock(rockType);
            chunk.blocks[lx][ly][lz].metadata = 0;
          } else if (wy <
                     config.seaLevel) { // VS: seaLevel is exclusive upper bound
            chunk.blocks[lx][ly][lz].block = waterBlock;
            chunk.blocks[lx][ly][lz].metadata = 0;
          } else {
            chunk.blocks[lx][ly][lz].block = airBlock;
            chunk.blocks[lx][ly][lz].metadata = 0;
          }
        }

        // 2. Surface Blocks
        float beachInfo = column.beachNoiseMap[lx][lz];

        if (surfaceHeight >= startY && surfaceHeight < startY + CHUNK_SIZE) {
          int localSurfaceY = surfaceHeight - startY;

          // Beach around sea level (+/- 2 blocks)
          bool isBeach = (surfaceHeight >= config.seaLevel - 2 &&
                          surfaceHeight <= config.seaLevel + 2) &&
                         (beachInfo > 0.2f);

          if (isBeach) {
            chunk.blocks[lx][localSurfaceY][lz].block = sandBlock;
            chunk.blocks[lx][localSurfaceY][lz].metadata = 0;
            if (localSurfaceY > 0) {
              chunk.blocks[lx][localSurfaceY - 1][lz].block = sandstoneBlock;
              chunk.blocks[lx][localSurfaceY - 1][lz].metadata = 0;
            }
          } else {
            if (surfaceHeight < config.seaLevel) {
              // Underwater terrain (gravel)
              chunk.blocks[lx][localSurfaceY][lz].block = gravelBlock;
              chunk.blocks[lx][localSurfaceY][lz].metadata = 0;
            } else {
              // Land
              chunk.blocks[lx][localSurfaceY][lz].block = grassBlock;
              chunk.blocks[lx][localSurfaceY][lz].metadata = 0;
              if (localSurfaceY > 0) {
                chunk.blocks[lx][localSurfaceY - 1][lz].block = dirtBlock;
                chunk.blocks[lx][localSurfaceY - 1][lz].metadata = 0;
              }
              if (localSurfaceY > 1) {
                chunk.blocks[lx][localSurfaceY - 2][lz].block = dirtBlock;
                chunk.blocks[lx][localSurfaceY - 2][lz].metadata = 0;
              }
              if (localSurfaceY > 2) {
                chunk.blocks[lx][localSurfaceY - 3][lz].block = dirtBlock;
                chunk.blocks[lx][localSurfaceY - 3][lz].metadata = 0;
              }
            }
          }
        }
      }
    }
  }

  // 3. Caves
  {
    PROFILE_SCOPE_CONDITIONAL("ChunkGen_Caves", m_ProfilingEnabled);
    caveGenerator->GenerateCaves(chunk, column, noiseManager);
  }

  // 4. Decorators
  {
    PROFILE_SCOPE_CONDITIONAL("ChunkGen_Decorators", m_ProfilingEnabled);
    for (auto *decorator : decorators) {
      decorator->Decorate(chunk, *this, column);
    }
  }
}

// Helpers
int WorldGenerator::GetHeight(int x, int z) {
  float upheaval = noiseManager.GetUpheaval(x, z);
  float hash1 = noiseManager.GetLandformNoise(x, z);
  float hash2 = noiseManager.GetLandformNeighborNoise(x, z);
  float hash3 = noiseManager.GetLandformNeighbor3Noise(x, z);

  float t = noiseManager.GetTemperature(x, z);
  float h = noiseManager.GetHumidity(x, z);

  int cellScale = std::max(1, (int)(1.0f / config.landformScale));
  int cellX = x / cellScale;
  int cellZ = z / cellScale;

  Landform lf1 = landformRegistry.Select(cellX, cellZ, t, h);
  Landform lf2 = lf1;
  Landform lf3 = lf1;

  float baseHeight = 64.0f + upheaval * 20.0f;
  float detail = noiseManager.GetTerrainDetail(x, z);

  auto calcHeight = [&](const Landform &lf) -> float {
    // 1. Accumulate Octaves
    float noiseSum = 0.0f;
    for (int i = 0; i < (int)lf.terrainOctaves.size(); ++i) {
      float octaveNoise = noiseManager.GetTerrainOctave((float)x, (float)z, i);
      // Filter by threshold
      float val = octaveNoise - lf.terrainOctaves[i].threshold;
      if (val < 0)
        val = 0; // VS often clamps or uses as hard threshold
      noiseSum += val * lf.terrainOctaves[i].amplitude * 20.0f; // Scale factor
    }

    // 2. Adjust for Ocean / Base
    float baseShift = 0.0f;
    if (lf.name.find("Ocean") != std::string::npos) {
      baseShift = -20.0f;
    }

    // 3. Find Surface Height via Binary Search of Density Thresholds
    // Simple heuristic: Height is where DensityThreshold(y) + noiseSum ~= 0
    // Note: Our convertsion th * 2 - 1 means -1 is dense, 1 is air.
    // So we want Threshold(y) + noiseSum/Scale >= 0 ?
    // Actually, in our converted system: Threshold 1.0 is air, -1.0 is solid.
    // Noise is positive. So we want Threshold(y) + NoiseSum/40.0f >= 0.

    int low = 1;
    int high = 318;
    int bestY = low;

    // Noise normalization for threshold comparison
    float normalizedNoise = noiseSum / 40.0f;

    while (low <= high) {
      int mid = low + (high - low) / 2;
      float density = lf.GetDensityThreshold(mid) + normalizedNoise;
      if (density >= 0) { // Air
        high = mid - 1;
      } else { // Solid
        bestY = mid;
        low = mid + 1;
      }
    }

    return (float)bestY + baseShift + upheaval * 10.0f;
  };

  float h1 = calcHeight(lf1);
  float h2 = calcHeight(lf2);
  float h3 = calcHeight(lf3);

  float f1, f2, f3;
  noiseManager.GetLandformDistances(x, z, f1, f2, f3);

  float R = 0.2f;

  float w1 = 1.0f;
  float w2 = 0.0f;
  float w3 = 0.0f;

  if (f2 - f1 < R) {
    float t2 = 1.0f - (f2 - f1) / R;
    w2 = t2 * t2 * t2 * (t2 * (t2 * 6.0f - 15.0f) + 10.0f);
  }
  if (f3 - f1 < R) {
    float t3 = 1.0f - (f3 - f1) / R;
    w3 = t3 * t3 * t3 * (t3 * (t3 * 6.0f - 15.0f) + 10.0f);
  }

  float totalW = w1 + w2 + w3;
  float finalSurfaceY = (h1 * w1 + h2 * w2 + h3 * w3) / totalW;

  float weight1 = w1 / totalW;
  if (weight1 < 1.0f) {
    float terrainDetail = noiseManager.GetTerrainDetail(x, z);
    finalSurfaceY += terrainDetail * 15.0f * (1.0f - weight1);
  }

  int surfaceY = (int)finalSurfaceY;
  if (surfaceY < 5)
    surfaceY = 5;
  if (surfaceY > 255)
    surfaceY = 255;
  return surfaceY;
}

std::string WorldGenerator::GetLandformNameAt(int x, int z) {
  float lfNoise = noiseManager.GetLandformNoise(x, z);
  float t = noiseManager.GetTemperature(x, z);
  float h = noiseManager.GetHumidity(x, z);
  int cellScale = std::max(1, (int)(1.0f / config.landformScale));
  int cellX = x / cellScale;
  int cellZ = z / cellScale;
  Landform lf = landformRegistry.Select(cellX, cellZ, t, h);
  return lf.name;
}

BlockType WorldGenerator::GetSurfaceBlock(int x, int y, int z,
                                          const ChunkColumn *column) {
  return BlockType::GRASS;
}

float WorldGenerator::GetTemperature(int x, int z) {
  float baseTemp = noiseManager.GetTemperature(x, z);
  int surfaceY = GetHeight(x, z);
  // Temperature lapse rate: colder at high altitudes
  float altitudeFactor = (float)(surfaceY - config.seaLevel);
  return baseTemp - (altitudeFactor * config.temperatureLapseRate);
}
float WorldGenerator::GetHumidity(int x, int z) {
  return noiseManager.GetHumidity(x, z);
}
float WorldGenerator::GetForestNoise(int x, int z) {
  return noiseManager.GetForestNoise(x, z);
}
float WorldGenerator::GetBushNoise(int x, int z) {
  return noiseManager.GetBushNoise(x, z);
}
float WorldGenerator::GetBeachNoise(int x, int z) {
  return noiseManager.GetBeachNoise(x, z);
}
float WorldGenerator::GetLandformNoise(int x, int z) {
  return noiseManager.GetLandformNoise(x, z);
}
