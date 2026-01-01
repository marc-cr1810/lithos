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
    : config(config), m_Seed(config.seed),
      noiseManager(config), // Initialize Noise Manager
      landformRegistry(LandformRegistry::Get()),
      strataRegistry(RockStrataRegistry::Get()) {
  // Initialize Decorators
  caveGenerator = new CaveGenerator(config);
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
  {
    PROFILE_SCOPE_CONDITIONAL("ChunkGen_Noise_Landform", m_ProfilingEnabled);
    noiseManager.GenLandform(landformNoise.data(), startX, startZ, CHUNK_SIZE,
                             CHUNK_SIZE);
  }
  {
    PROFILE_SCOPE_CONDITIONAL("ChunkGen_Noise_LandformNeighbor",
                              m_ProfilingEnabled);
    noiseManager.GenLandformNeighbor(landformNeighbor.data(), startX, startZ,
                                     CHUNK_SIZE, CHUNK_SIZE);
    noiseManager.GenLandformNeighbor3(landformNeighbor3.data(), startX, startZ,
                                      CHUNK_SIZE, CHUNK_SIZE);
  }
  {
    PROFILE_SCOPE_CONDITIONAL("ChunkGen_Noise_LandformDistances",
                              m_ProfilingEnabled);
    noiseManager.GenLandformDistances(landformF1.data(), landformF2.data(),
                                      landformF3.data(), startX, startZ,
                                      CHUNK_SIZE, CHUNK_SIZE);
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
  {
    PROFILE_SCOPE_CONDITIONAL("ChunkGen_Noise_LandformEdge",
                              m_ProfilingEnabled);
    noiseManager.GenLandformEdge(edgeNoise.data(), startX, startZ, CHUNK_SIZE,
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
      float hash1 = landformNoise[index];
      float hash2 = landformNeighbor[index];
      float hash3 = landformNeighbor3[index];

      float t = tempMap[index];
      float h = humidMap[index];

      Landform lf1 = landformRegistry.Select(hash1, t, h);
      Landform lf2 = landformRegistry.Select(hash2, t, h);
      Landform lf3 = landformRegistry.Select(hash3, t, h);

      float upVal = upheaval[index];
      float baseHeight = 64.0f + upVal * 20.0f;
      float detail = terrainDetail[index];

      // Lambda for height calculation
      auto calcHeight = [&](const Landform &lf) -> float {
        float hMod;
        if (!lf.terrainOctaves.empty()) {
          hMod =
              ((detail + 1.0f) * 0.5f) * lf.terrainOctaves[0].amplitude * 80.0f;
        } else {
          hMod = ((detail + 1.0f) * 0.5f) * 20.0f;
        }
        float surfaceY = baseHeight;
        if (lf.name.find("Ocean") != std::string::npos) {
          surfaceY = config.seaLevel - 15.0f;
          hMod *= 0.5f;
        }
        return surfaceY + hMod;
      };

      float h1 = calcHeight(lf1);
      float h2 = calcHeight(lf2);
      float h3 = calcHeight(lf3);

      // --- ADAPTIVE BLEND RADIUS ---
      // Use larger radius for similar landforms, smaller for very different
      // ones
      float heightDiff12 = std::abs(h2 - h1);
      float heightDiff13 = std::abs(h3 - h1);

      // Base radius scales with height similarity
      // Similar heights (< 10 blocks diff) -> wider blend (0.5)
      // Very different (> 40 blocks diff) -> sharp transition (0.15)
      float avgDiff = (heightDiff12 + heightDiff13) * 0.5f;
      float R = 0.5f - (avgDiff / 40.0f) * 0.35f;
      R = std::max(0.15f, std::min(0.5f, R)); // Clamp between 0.15 and 0.5

      // Get Voronoi distances for blending
      float f1 = landformF1[index];
      float f2 = landformF2[index];
      float f3 = landformF3[index];

      float w1 = 1.0f;
      float w2 = 0.0f;
      float w3 = 0.0f;

      if (f2 - f1 < R) {
        float t2 = 1.0f - (f2 - f1) / R;
        w2 = t2 * t2 * t2 * (t2 * (t2 * 6.0f - 15.0f) + 10.0f); // Quintic
      }

      if (f3 - f1 < R) {
        float t3 = 1.0f - (f3 - f1) / R;
        w3 = t3 * t3 * t3 * (t3 * (t3 * 6.0f - 15.0f) + 10.0f); // Quintic
      }

      float totalW = w1 + w2 + w3;
      float finalSurfaceY = (h1 * w1 + h2 * w2 + h3 * w3) / totalW;

      // --- REDUCED TRANSITION JITTER ---
      // Gentle detail variation instead of harsh jitter
      float weight1 = w1 / totalW;
      if (weight1 < 1.0f) {
        float blendFactor = 1.0f - weight1;
        float jitter =
            terrainDetail[index] * 3.0f * blendFactor; // Reduced from 15.0f
        finalSurfaceY += jitter;
      }

      // Clamp
      int surfaceY = (int)finalSurfaceY;

      if (surfaceY < 5)
        surfaceY = 5;
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

  // Regnerate Province Noise (since it wasn't stored in column explicitly, or
  // we could add it)
  static thread_local std::vector<float> provinceNoise(CHUNK_SIZE * CHUNK_SIZE);
  noiseManager.GenGeologic(provinceNoise.data(), startX, startZ, CHUNK_SIZE,
                           CHUNK_SIZE);

  // Generate Strata Noise (Smooth layers)
  static thread_local std::vector<float> strataNoise(CHUNK_SIZE * CHUNK_SIZE);
  noiseManager.GenStrata(strataNoise.data(), startX, startZ, CHUNK_SIZE,
                         CHUNK_SIZE);

  {
    PROFILE_SCOPE_CONDITIONAL("ChunkGen_Terrain", m_ProfilingEnabled);

    // Cache common blocks for direct access
    Block *waterBlock = BlockRegistry::getInstance().getBlock(BlockType::WATER);
    Block *airBlock = BlockRegistry::getInstance().getBlock(BlockType::AIR);
    Block *sandBlock = BlockRegistry::getInstance().getBlock(BlockType::SAND);
    Block *sandstoneBlock =
        BlockRegistry::getInstance().getBlock(BlockType::SANDSTONE);
    Block *gravelBlock =
        BlockRegistry::getInstance().getBlock(BlockType::GRAVEL);
    Block *grassBlock = BlockRegistry::getInstance().getBlock(BlockType::GRASS);
    Block *dirtBlock = BlockRegistry::getInstance().getBlock(BlockType::DIRT);

    for (int lx = 0; lx < CHUNK_SIZE; lx++) {
      for (int lz = 0; lz < CHUNK_SIZE; lz++) {
        int wx = startX + lx;
        int wz = startZ + lz;
        int index = lx + lz * CHUNK_SIZE;

        int surfaceHeight = column.getHeight(lx, lz);
        float pNoise = provinceNoise[index];
        float sNoise = strataNoise[index];

        for (int ly = 0; ly < CHUNK_SIZE; ly++) {
          int wy = startY + ly;

          // 1. Base Terrain (Rock Strata)
          if (wy <= surfaceHeight) {
            BlockType rockType = strataRegistry.GetStrataBlock(
                wx, wy, wz, surfaceHeight, pNoise, sNoise, m_Seed);
            chunk.blocks[lx][ly][lz].block =
                BlockRegistry::getInstance().getBlock(rockType);
            chunk.blocks[lx][ly][lz].metadata = 0;
          } else if (wy <= config.seaLevel) {
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
              chunk.blocks[lx][localSurfaceY][lz].block = gravelBlock;
              chunk.blocks[lx][localSurfaceY][lz].metadata = 0;
            } else {
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

  Landform lf1 = landformRegistry.Select(hash1, t, h);
  Landform lf2 = landformRegistry.Select(hash2, t, h);
  Landform lf3 = landformRegistry.Select(hash3, t, h);

  float baseHeight = 64.0f + upheaval * 20.0f;
  float detail = noiseManager.GetTerrainDetail(x, z);

  auto calcHeight = [&](const Landform &lf) -> float {
    float hMod;
    if (!lf.terrainOctaves.empty()) {
      hMod = ((detail + 1.0f) * 0.5f) * lf.terrainOctaves[0].amplitude * 80.0f;
    } else {
      hMod = ((detail + 1.0f) * 0.5f) * 20.0f;
    }
    float surfaceY = baseHeight;
    if (lf.name.find("Ocean") != std::string::npos) {
      surfaceY = config.seaLevel - 15.0f;
      hMod *= 0.5f;
    }
    return surfaceY + hMod;
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
  Landform lf = landformRegistry.Select(lfNoise, t, h);
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
