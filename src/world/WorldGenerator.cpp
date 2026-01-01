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

  // 1. Generate Noise Grids
  static thread_local std::vector<float> upheaval(CHUNK_SIZE * CHUNK_SIZE);
  static thread_local std::vector<float> landformNoise(CHUNK_SIZE * CHUNK_SIZE);
  static thread_local std::vector<float> provinceNoise(CHUNK_SIZE * CHUNK_SIZE);
  static thread_local std::vector<float> tempMap(CHUNK_SIZE * CHUNK_SIZE);
  static thread_local std::vector<float> humidMap(CHUNK_SIZE * CHUNK_SIZE);
  static thread_local std::vector<float> terrainDetail(CHUNK_SIZE * CHUNK_SIZE);
  static thread_local std::vector<float> edgeNoise(CHUNK_SIZE *
                                                   CHUNK_SIZE); // New

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
    PROFILE_SCOPE_CONDITIONAL("ChunkGen_Noise_LandformEdge",
                              m_ProfilingEnabled);
    noiseManager.GenLandformEdge(edgeNoise.data(), startX, startZ, CHUNK_SIZE,
                                 CHUNK_SIZE); // New
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

  // Decorator data
  static thread_local std::vector<float> forestMap(CHUNK_SIZE * CHUNK_SIZE);
  static thread_local std::vector<float> bushMap(CHUNK_SIZE * CHUNK_SIZE);
  {
    PROFILE_SCOPE_CONDITIONAL("ChunkGen_Noise_Vegetation", m_ProfilingEnabled);
    noiseManager.GenVegetation(forestMap.data(), bushMap.data(), startX, startZ,
                               CHUNK_SIZE, CHUNK_SIZE);
  }

  static thread_local std::vector<float> beachMap(CHUNK_SIZE * CHUNK_SIZE);
  {
    PROFILE_SCOPE_CONDITIONAL("ChunkGen_Noise_Beach", m_ProfilingEnabled);
    noiseManager.GenBeach(beachMap.data(), startX, startZ, CHUNK_SIZE,
                          CHUNK_SIZE);
  }

  // 2. Process Column Data
  {
    PROFILE_SCOPE_CONDITIONAL("ChunkGen_Column_Processing", m_ProfilingEnabled);
    for (int z = 0; z < CHUNK_SIZE; z++) {
      for (int x = 0; x < CHUNK_SIZE; x++) {
        int index = x + z * CHUNK_SIZE;
        int worldX = startX + x;
        int worldZ = startZ + z;

        // Fill Column Data
        column.temperatureMap[x][z] = tempMap[index];
        column.humidityMap[x][z] = humidMap[index];
        column.forestNoiseMap[x][z] = forestMap[index];
        column.bushNoiseMap[x][z] = bushMap[index];
        column.beachNoiseMap[x][z] = beachMap[index];

        // Select Landform
        // Select Landform
        // Select Landform
        Landform lf = landformRegistry.Select(landformNoise[index],
                                              tempMap[index], humidMap[index]);

        // Calculate Height
        // Use Terrain Detail to drive height splines
        // Scale detail by landform's amplitude (first octave)
        float detailAmp = 1.0f;
        if (!lf.terrainOctaves.empty()) {
          detailAmp = lf.terrainOctaves[0].amplitude;
        }

        float rawDetail = terrainDetail[index] * detailAmp;
        float targetThreshold = -rawDetail;
        float surfaceYFloat = 0.0f;

        // Ensure keys exist (they should, based on Landform registry)
        if (lf.yKeys.empty()) {
          // Fallback if no keys
          surfaceYFloat = 64.0f + rawDetail * 20.0f;
        } else {
          // Find segment traversing targetThreshold
          bool found = false;
          for (size_t i = 0; i < lf.yKeys.size() - 1; ++i) {
            float t1 = lf.yKeys[i].threshold;
            float t2 = lf.yKeys[i + 1].threshold;
            int y1 = lf.yKeys[i].yLevel;
            int y2 = lf.yKeys[i + 1].yLevel;

            // Check if target is between t1 and t2
            if ((targetThreshold <= t1 && targetThreshold >= t2) ||
                (targetThreshold >= t1 && targetThreshold <= t2)) {

              float alpha = (targetThreshold - t1) / (t2 - t1);
              surfaceYFloat = y1 + alpha * (y2 - y1);
              found = true;
              break;
            }
          }

          // Clamping if outside range
          if (!found) {
            if (targetThreshold > lf.yKeys.front().threshold)
              surfaceYFloat = (float)lf.yKeys.front().yLevel;
            else
              surfaceYFloat = (float)lf.yKeys.back().yLevel;
          }
        }

        // Modifier from Upheaval (optional, adds extra bumps)
        surfaceYFloat += upheaval[index] * 10.0f;

        // EDGE BLENDING: Fade to baseline (64) at biome borders
        float edgeVal = edgeNoise[index]; // Distance F2-F1

        float blendFactor = edgeVal;
        // Sharpen the blend curve so flattening only happens VERY close to edge
        blendFactor = (blendFactor > 1.0f) ? 1.0f : blendFactor;
        blendFactor = blendFactor * 2.0f; // Scale up to reach 1 faster
        if (blendFactor > 1.0f)
          blendFactor = 1.0f;

        // Lerp towards Base Height (64) as blendFactor approaches 0
        surfaceYFloat = 64.0f + (surfaceYFloat - 64.0f) * blendFactor;

        // Smoothing: Clamp extremely low/high values to keep within reasonable
        // bounds if needed But noise scaling should handle this naturally now.

        int surfaceY = (int)surfaceYFloat;

        // Clamp
        if (surfaceY < 5)
          surfaceY = 5;
        if (surfaceY > 255)
          surfaceY = 255;

        column.setHeight(x, z, surfaceY);
      }
    }
  }
}

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
  float lfNoise = noiseManager.GetLandformNoise(x, z);
  float t = noiseManager.GetTemperature(x, z);
  float h = noiseManager.GetHumidity(x, z);

  Landform lf = landformRegistry.Select(lfNoise, t, h);

  float baseHeight = 64.0f + upheaval * 20.0f;
  float terrainNoise = (lfNoise + 1.0f) * 0.5f;

  float heightMod = 0.0f;
  if (!lf.terrainOctaves.empty()) {
    heightMod = terrainNoise * lf.terrainOctaves[0].amplitude * 80.0f;
  } else {
    heightMod = terrainNoise * 20.0f;
  }

  if (lf.name.find("Ocean") != std::string::npos) {
    baseHeight = config.seaLevel - 15.0f;
    heightMod *= 0.5f;
  }

  int surfaceY = (int)(baseHeight + heightMod);

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
  return noiseManager.GetTemperature(x, z);
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
