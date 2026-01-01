#include "WorldGenerator.h"
#include "../debug/Logger.h"
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
  int startX = cx * CHUNK_SIZE;
  int startZ = cz * CHUNK_SIZE;

  // 1. Generate Noise Grids
  static thread_local std::vector<float> upheaval(CHUNK_SIZE * CHUNK_SIZE);
  static thread_local std::vector<float> landformNoise(CHUNK_SIZE * CHUNK_SIZE);
  static thread_local std::vector<float> provinceNoise(CHUNK_SIZE * CHUNK_SIZE);
  static thread_local std::vector<float> tempMap(CHUNK_SIZE * CHUNK_SIZE);
  static thread_local std::vector<float> humidMap(CHUNK_SIZE * CHUNK_SIZE);
  static thread_local std::vector<float> terrainDetail(CHUNK_SIZE *
                                                       CHUNK_SIZE); // New

  noiseManager.GenUpheaval(upheaval.data(), startX, startZ, CHUNK_SIZE,
                           CHUNK_SIZE);
  noiseManager.GenLandform(landformNoise.data(), startX, startZ, CHUNK_SIZE,
                           CHUNK_SIZE);
  noiseManager.GenGeologic(provinceNoise.data(), startX, startZ, CHUNK_SIZE,
                           CHUNK_SIZE);
  noiseManager.GenClimate(tempMap.data(), humidMap.data(), startX, startZ,
                          CHUNK_SIZE, CHUNK_SIZE);
  noiseManager.GenTerrainDetail(terrainDetail.data(), startX, startZ,
                                CHUNK_SIZE,
                                CHUNK_SIZE); // New

  // Decorator data
  static thread_local std::vector<float> forestMap(CHUNK_SIZE * CHUNK_SIZE);
  static thread_local std::vector<float> bushMap(CHUNK_SIZE * CHUNK_SIZE);
  noiseManager.GenVegetation(forestMap.data(), bushMap.data(), startX, startZ,
                             CHUNK_SIZE, CHUNK_SIZE);

  static thread_local std::vector<float> beachMap(CHUNK_SIZE * CHUNK_SIZE);
  noiseManager.GenBeach(beachMap.data(), startX, startZ, CHUNK_SIZE,
                        CHUNK_SIZE);

  // 2. Process Column Data
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

void WorldGenerator::GenerateChunk(Chunk &chunk, const ChunkColumn &column) {
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

  for (int lx = 0; lx < CHUNK_SIZE; lx++) {
    for (int lz = 0; lz < CHUNK_SIZE; lz++) {
      int wx = startX + lx;
      int wz = startZ + lz;
      int index = lx + lz * CHUNK_SIZE; // Fixed map access

      int surfaceHeight = column.getHeight(lx, lz);
      float pNoise = provinceNoise[index];

      for (int ly = 0; ly < CHUNK_SIZE; ly++) {
        int wy = startY + ly;

        // 1. Base Terrain (Rock Strata)
        if (wy <= surfaceHeight) {
          BlockType rock = strataRegistry.GetStrataBlock(
              wx, wy, wz, surfaceHeight, pNoise, m_Seed);
          chunk.setBlock(lx, ly, lz, rock);
        } else if (wy <= config.seaLevel) {
          chunk.setBlock(lx, ly, lz, BlockType::WATER);
        } else {
          chunk.setBlock(lx, ly, lz, BlockType::AIR);
        }
      }

      // 2. Surface Blocks
      float beachInfo = column.beachNoiseMap[lx][lz]; // Using column data now

      if (surfaceHeight >= startY && surfaceHeight < startY + CHUNK_SIZE) {
        int localSurfaceY = surfaceHeight - startY;

        bool isBeach = (surfaceHeight >= config.seaLevel - 2 &&
                        surfaceHeight <= config.seaLevel + 2) &&
                       (beachInfo > 0.2f);

        if (isBeach) {
          chunk.setBlock(lx, localSurfaceY, lz, BlockType::SAND);
          if (localSurfaceY > 0)
            chunk.setBlock(lx, localSurfaceY - 1, lz, BlockType::SANDSTONE);
        } else {
          if (surfaceHeight < config.seaLevel) {
            chunk.setBlock(lx, localSurfaceY, lz, BlockType::GRAVEL);
          } else {
            chunk.setBlock(lx, localSurfaceY, lz, BlockType::GRASS);
            if (localSurfaceY > 0)
              chunk.setBlock(lx, localSurfaceY - 1, lz, BlockType::DIRT);
            if (localSurfaceY > 1)
              chunk.setBlock(lx, localSurfaceY - 2, lz, BlockType::DIRT);
            if (localSurfaceY > 2)
              chunk.setBlock(lx, localSurfaceY - 3, lz, BlockType::DIRT);
          }
        }
      }
    }
  }

  // 3. Caves
  for (int lx = 0; lx < CHUNK_SIZE; lx++) {
    for (int ly = 0; ly < CHUNK_SIZE; ly++) {
      for (int lz = 0; lz < CHUNK_SIZE; lz++) {
        int wx = startX + lx;
        int wy = startY + ly;
        int wz = startZ + lz;

        if (wy <= 5)
          continue;

        if (caveGenerator->IsCaveAt(wx, wy, wz, config.worldHeight)) {
          // Verify we aren't cutting into water/ocean
          BlockType current =
              static_cast<BlockType>(chunk.getBlock(lx, ly, lz).getType());
          if (current == WATER || current == ICE)
            continue;

          if (wy < config.lavaLevel) {
            chunk.setBlock(lx, ly, lz, BlockType::LAVA);
          } else {
            chunk.setBlock(lx, ly, lz, BlockType::AIR);
          }
        }
      }
    }
  }

  // 4. Decorators
  for (auto *decorator : decorators) {
    decorator->Decorate(chunk, *this, column);
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
