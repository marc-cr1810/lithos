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
#include <glm/glm.hpp>
#include <iostream>
#include <queue>

WorldGenerator::WorldGenerator(const WorldGenConfig &config)
    : config(config), m_Seed(config.seed), noiseManager(config),
      landformRegistry(LandformRegistry::Get()),
      strataRegistry(RockStrataRegistry::Get()) {
  // VS Style Sea Level: ~43% of world height
  this->config.seaLevel = (int)(config.worldHeight * 0.4313725490196078);

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
      const Landform *lf1 = landformRegistry.Select(landformNoise[index], t, h);
      const Landform *lf2 =
          landformRegistry.Select(landformNeighbor[index], t, h);
      const Landform *lf3 =
          landformRegistry.Select(landformNeighbor3[index], t, h);

      float upVal = noiseManager.GetUpheaval(wx, wz);
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

      // Simple noise generation (baseline)
      static thread_local std::vector<float> noiseColumn(320);
      if ((int)noiseColumn.size() < config.worldHeight)
        noiseColumn.resize(config.worldHeight);

      noiseManager.GenTerrainNoise3D(noiseColumn.data(), wx, 0, wz, 1,
                                     config.worldHeight, 1);

      // Optimization: Hoist LUT pointers
      const std::vector<float> *lut1Ptr = lf1->GetLUT();
      const float *lut1Data = lut1Ptr ? lut1Ptr->data() : nullptr;
      int lut1Size = lut1Ptr ? (int)lut1Ptr->size() : 0;

      const std::vector<float> *lut2Ptr = lf2->GetLUT();
      const float *lut2Data = lut2Ptr ? lut2Ptr->data() : nullptr;
      int lut2Size = lut2Ptr ? (int)lut2Ptr->size() : 0;

      const std::vector<float> *lut3Ptr = lf3->GetLUT();
      const float *lut3Data = lut3Ptr ? lut3Ptr->data() : nullptr;
      int lut3Size = lut3Ptr ? (int)lut3Ptr->size() : 0;

      // Optimization: Coarse Search
      const int coarseStep = 4;

      // Start from top, step down by coarseStep
      for (int y = config.worldHeight - 1; y > 0; y -= coarseStep) {

        // Helper to calculate density at specific Y
        auto getDensity = [&](int sampleY) -> float {
          // Apply Upheaval
          int shiftedY = (int)(sampleY - (upVal * 40.0f));

          float th1, th2, th3;

          if (lut1Data && shiftedY >= 0 && shiftedY < lut1Size)
            th1 = lut1Data[shiftedY];
          else
            th1 = lf1->GetDensityThreshold(shiftedY);

          if (lut2Data && shiftedY >= 0 && shiftedY < lut2Size)
            th2 = lut2Data[shiftedY];
          else
            th2 = lf2->GetDensityThreshold(shiftedY);

          if (lut3Data && shiftedY >= 0 && shiftedY < lut3Size)
            th3 = lut3Data[shiftedY];
          else
            th3 = lf3->GetDensityThreshold(shiftedY);

          float th = th1 * w1 + th2 * w2 + th3 * w3;

          // Optimization: Early Exit
          if (th > 1.2f)
            return 1.0f; // Definitely Solid
          if (th < -1.2f)
            return -1.0f; // Definitely Air

          // **Per-Octave Noise Processing (VS-Style)**
          // Generate 9 octaves and filter each with max(0, noise - threshold)

          // Simple baseline: single noise value
          float n = 0.0f;
          if (sampleY >= 0 && sampleY < config.worldHeight) {
            n = noiseColumn[sampleY];
          } else {
            n = noiseManager.GetTerrainNoise3D(wx, sampleY, wz);
          }

          return n + th;
        };

        float density = getDensity(y);

        if (density > 0) { // Found solid (or close to it)
          // We found a solid block at 'y'.
          // The surface is likely between 'y' and 'y + coarseStep'.
          // We need to check upwards from y to find the *first* solid block
          // from top (highest solid). Actually we are searching *downwards*. If
          // 'y' is solid, the transition from Air -> Solid happened above. So
          // we check y+1, y+2, y+3... upwards? Or rather, we know y+coarseStep
          // was Air (from previous iteration). So we check range [y +
          // coarseStep - 1] down to [y].

          int topSearch = std::min(config.worldHeight - 1, y + coarseStep - 1);

          bool foundPrecise = false;
          for (int preciseY = topSearch; preciseY >= y; preciseY--) {
            if (getDensity(preciseY) > 0) {
              surfaceY = preciseY;
              foundPrecise = true;
              break;
            }
          }

          if (foundPrecise)
            break;
        }
      }

      // Clamp
      if (surfaceY < 5)
        surfaceY = 5; // Bedrock
      if (surfaceY > 255)
        surfaceY = 255; // Clamped to 255 for Column storage? Wait/Check column
                        // height storage.

      // Debug SurfaceY
      if (lx == 8 && lz == 8 &&
          index == 0) { // Log once per column gen batch or similar
        LOG_INFO("GenColumn: surfaceY at ({}, {}) = {}", wx, wz, surfaceY);
      }

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

  // We also need temp/humid for selection
  static thread_local std::vector<float> tempMap(CHUNK_SIZE * CHUNK_SIZE);
  static thread_local std::vector<float> humidMap(CHUNK_SIZE * CHUNK_SIZE);

  noiseManager.GenLandformComposite(
      landformNoise.data(), landformNeighbor.data(), landformNeighbor3.data(),
      landformF1.data(), landformF2.data(), landformF3.data(), nullptr, startX,
      startZ, CHUNK_SIZE, CHUNK_SIZE);

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
    Block *mudBlock = BlockRegistry::getInstance().getBlock(BlockType::MUD);
    Block *podzolBlock =
        BlockRegistry::getInstance().getBlock(BlockType::PODZOL);
    Block *coarseDirtBlock =
        BlockRegistry::getInstance().getBlock(BlockType::COARSE_DIRT);
    Block *terraPretaBlock =
        BlockRegistry::getInstance().getBlock(BlockType::TERRA_PRETA);
    Block *peatBlock = BlockRegistry::getInstance().getBlock(BlockType::PEAT);

    // Lambda to get density (Similar to GenerateColumn but for specific y)

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
        const Landform *lf1 = landformRegistry.Select(
            landformNoise[index], tempMap[index], humidMap[index]);
        const Landform *lf2 = landformRegistry.Select(
            landformNeighbor[index], tempMap[index], humidMap[index]);
        const Landform *lf3 = landformRegistry.Select(
            landformNeighbor3[index], tempMap[index], humidMap[index]);

        int surfaceHeight = column.getHeight(lx, lz);

        // Use direct sampling to ensure consistency with GetHeight and avoid
        // array index/generation mismatches
        float pNoise = noiseManager.GetGeologicNoise(wx, wz);
        float sNoise = noiseManager.GetStrata(wx, wz);
        float upVal = noiseManager.GetUpheaval(wx, wz);

        // Optimization: Pre-calculate density thresholds for this column slice
        // Only needed if we are below surface height
        float columnThresholds[CHUNK_SIZE];

        // Determine how many blocks need density check
        // We only check density if wy <= surfaceHeight
        // wy = startY + ly.  startY + ly <= surfaceHeight  => ly <=
        // surfaceHeight - startY
        int processLimit = surfaceHeight - startY;

        if (processLimit >= 0) {
          int maxLy = std::min(CHUNK_SIZE - 1, processLimit);

          const std::vector<float> *lut1Ptr = lf1->GetLUT();
          const float *lut1Data = lut1Ptr ? lut1Ptr->data() : nullptr;
          int lut1Size = lut1Ptr ? (int)lut1Ptr->size() : 0;

          const std::vector<float> *lut2Ptr = lf2->GetLUT();
          const float *lut2Data = lut2Ptr ? lut2Ptr->data() : nullptr;
          int lut2Size = lut2Ptr ? (int)lut2Ptr->size() : 0;

          const std::vector<float> *lut3Ptr = lf3->GetLUT();
          const float *lut3Data = lut3Ptr ? lut3Ptr->data() : nullptr;
          int lut3Size = lut3Ptr ? (int)lut3Ptr->size() : 0;

          // Generate Landform Densities
          for (int ly = 0; ly <= maxLy; ly++) {
            int wy = startY + ly;
            int sampleY = (int)(wy - (upVal * 40.0f));

            float th1, th2, th3;

            if (lut1Data && sampleY >= 0 && sampleY < lut1Size)
              th1 = lut1Data[sampleY];
            else
              th1 = lf1->GetDensityThreshold(sampleY);

            if (lut2Data && sampleY >= 0 && sampleY < lut2Size)
              th2 = lut2Data[sampleY];
            else
              th2 = lf2->GetDensityThreshold(sampleY);

            if (lut3Data && sampleY >= 0 && sampleY < lut3Size)
              th3 = lut3Data[sampleY];
            else
              th3 = lf3->GetDensityThreshold(sampleY);

            columnThresholds[ly] = th1 * w1 + th2 * w2 + th3 * w3;
          }

          // Generate 9 octaves for this column
          const int numOctaves = 9;
          static thread_local std::vector<float> octaveBuffer(CHUNK_SIZE *
                                                              numOctaves);
          if ((int)octaveBuffer.size() < (maxLy + 1) * numOctaves)
            octaveBuffer.resize((maxLy + 1) * numOctaves);

          noiseManager.GenTerrainNoise3DOctaves(
              octaveBuffer.data(), wx, startY, wz, 1, maxLy + 1, 1, numOctaves);

          // Pre-blend octave parameters once for the column
          float blendedAmps[9];
          float blendedThreshs[9];

          for (int octIdx = 0; octIdx < numOctaves; octIdx++) {
            float amp1 = (octIdx < (int)lf1->terrainOctaves.size())
                             ? lf1->terrainOctaves[octIdx].amplitude
                             : 0.0f;
            float amp2 = (octIdx < (int)lf2->terrainOctaves.size())
                             ? lf2->terrainOctaves[octIdx].amplitude
                             : 0.0f;
            float amp3 = (octIdx < (int)lf3->terrainOctaves.size())
                             ? lf3->terrainOctaves[octIdx].amplitude
                             : 0.0f;
            blendedAmps[octIdx] = amp1 * w1 + amp2 * w2 + amp3 * w3;

            float th1 = (octIdx < (int)lf1->terrainOctaves.size())
                            ? lf1->terrainOctaves[octIdx].threshold
                            : 0.0f;
            float th2 = (octIdx < (int)lf2->terrainOctaves.size())
                            ? lf2->terrainOctaves[octIdx].threshold
                            : 0.0f;
            float th3 = (octIdx < (int)lf3->terrainOctaves.size())
                            ? lf3->terrainOctaves[octIdx].threshold
                            : 0.0f;
            blendedThreshs[octIdx] = th1 * w1 + th2 * w2 + th3 * w3;
          }

          for (int ly = 0; ly < CHUNK_SIZE; ly++) {
            int wy = startY + ly;
            bool isSolid = false;

            if (ly <= maxLy) {
              float threshold = columnThresholds[ly];
              if (threshold > 1.2f) {
                isSolid = true;
              } else if (threshold < -1.2f) {
                isSolid = false;
              } else {
                // Per-octave filtering
                float noiseSum = 0.0f;

                for (int octIdx = 0; octIdx < numOctaves; octIdx++) {
                  if (blendedAmps[octIdx] == 0.0f)
                    continue;

                  // Get actual octave noise from buffer
                  float octaveNoise = octaveBuffer[ly * numOctaves + octIdx];

                  // DIAGNOSTIC: Disable threshold filtering
                  // float filtered = std::max(0.0f, octaveNoise -
                  // blendedThreshs[octIdx]);
                  float filtered = octaveNoise;
                  noiseSum += filtered * blendedAmps[octIdx];
                }

                // Simple comparison: noise + landform threshold
                isSolid = (noiseSum + threshold) > 0;
              }
            }

            // Block Placement Logic inside loop
            if (isSolid) {
              BlockType rockType = strataRegistry.GetStrataBlock(
                  wx, wy, wz, surfaceHeight, pNoise, sNoise, upVal, m_Seed);
              chunk.blocks[lx][ly][lz].block =
                  BlockRegistry::getInstance().getBlock(rockType);
              chunk.blocks[lx][ly][lz].metadata = 0;
            } else if (wy < config.seaLevel) {
              chunk.blocks[lx][ly][lz].block = waterBlock;
              chunk.blocks[lx][ly][lz].metadata = 0;
            } else {
              chunk.blocks[lx][ly][lz].block = airBlock;
              chunk.blocks[lx][ly][lz].metadata = 0;
            }
          }
        } else {
          // ProcessLimit < 0 means surface is below startY (Air chunk or water
          // chunk if below sea level) Just fill air/water
          for (int ly = 0; ly < CHUNK_SIZE; ly++) {
            int wy = startY + ly;
            if (wy < config.seaLevel) {
              chunk.blocks[lx][ly][lz].block = waterBlock;
              chunk.blocks[lx][ly][lz].metadata = 0;
            } else {
              chunk.blocks[lx][ly][lz].block = airBlock;
              chunk.blocks[lx][ly][lz].metadata = 0;
            }
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
              // Use Dynamic Surface Block
              BlockType surfaceType =
                  GetSurfaceBlock(wx, surfaceHeight, wz, &column);
              Block *surfaceBlock =
                  BlockRegistry::getInstance().getBlock(surfaceType);

              chunk.blocks[lx][localSurfaceY][lz].block = surfaceBlock;
              chunk.blocks[lx][localSurfaceY][lz].metadata = 0;

              // Determine Sub-surface block (default dirt)
              Block *subBlock = dirtBlock;
              if (surfaceType == BlockType::SAND)
                subBlock = sandBlock; // Or Sandstone?
              else if (surfaceType == BlockType::GRAVEL)
                subBlock = gravelBlock;
              else if (surfaceType == BlockType::MUD)
                subBlock = mudBlock;
              else if (surfaceType == BlockType::COARSE_DIRT)
                subBlock = coarseDirtBlock;
              else if (surfaceType == BlockType::PEAT)
                subBlock = peatBlock;

              if (localSurfaceY > 0) {
                chunk.blocks[lx][localSurfaceY - 1][lz].block = subBlock;
                chunk.blocks[lx][localSurfaceY - 1][lz].metadata = 0;
              }
              if (localSurfaceY > 1) {
                chunk.blocks[lx][localSurfaceY - 2][lz].block = subBlock;
                chunk.blocks[lx][localSurfaceY - 2][lz].metadata = 0;
              }
              if (localSurfaceY > 2) {
                chunk.blocks[lx][localSurfaceY - 3][lz].block = subBlock;
                chunk.blocks[lx][localSurfaceY - 3][lz].metadata = 0;
              }
            }
          }
        }
      }
    }
  }

  // 2.5. Liquid Surface Rules (Ice)
  // After all water is placed, check top layer for freezing
  {
    PROFILE_SCOPE_CONDITIONAL("ChunkGen_LiquidSurface", m_ProfilingEnabled);
    for (int lx = 0; lx < CHUNK_SIZE; lx++) {
      for (int lz = 0; lz < CHUNK_SIZE; lz++) {
        int wx = startX + lx;
        int wz = startZ + lz;
        int surfaceHeight = column.heightMap[lx][lz];

        // Check if there's water at or near sea level
        for (int ly = 0; ly < CHUNK_SIZE; ly++) {
          int wy = startY + ly;

          // Only process water at the top of water columns (surface)
          if (chunk.blocks[lx][ly][lz].block->getId() == BlockType::WATER) {
            // Check if this is the top water block (air or nothing above)
            bool isTopWater = false;
            if (ly == CHUNK_SIZE - 1) {
              isTopWater = true; // Top of chunk
            } else if (chunk.blocks[lx][ly + 1][lz].block->getId() ==
                       BlockType::AIR) {
              isTopWater = true; // Air above
            }

            if (isTopWater) {
              // Query liquid surface rules
              float temp = column.temperatureMap[lx][lz];
              float humid = column.humidityMap[lx][lz];
              float patch = noiseManager.GetSurfacePatchNoise(wx, wz);
              float fertility = humid;
              float yNormalized = (float)wy / (float)config.worldHeight;

              uint8_t liquidId =
                  BlockLayerConfig::Get().GetLiquidSurfaceBlockId(
                      temp, humid, fertility, patch, yNormalized);

              if (liquidId != BlockType::WATER) {
                chunk.blocks[lx][ly][lz].block =
                    BlockRegistry::getInstance().getBlock(liquidId);
                chunk.blocks[lx][ly][lz].metadata = 0;
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

  // 5. Post-Processing
  {
    PROFILE_SCOPE_CONDITIONAL("ChunkGen_PostProcess", m_ProfilingEnabled);
    CleanupFloatingIslands(chunk);
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

  const Landform *lf1 = landformRegistry.Select(hash1, t, h);
  const Landform *lf2 = landformRegistry.Select(hash2, t, h);
  const Landform *lf3 = landformRegistry.Select(hash3, t, h);

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

  float h1 = calcHeight(*lf1);
  float h2 = calcHeight(*lf2);
  float h3 = calcHeight(*lf3);

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
  const Landform *lf = landformRegistry.Select(cellX, cellZ, t, h);
  return lf->name;
}

BlockType WorldGenerator::GetSurfaceBlock(int x, int y, int z,
                                          const ChunkColumn *column) {
  // Use config loader by default
  float temp = noiseManager.GetTemperature(x, z);
  float humid = noiseManager.GetHumidity(x, z);
  float patch = noiseManager.GetSurfacePatchNoise(x, z);
  float beach = noiseManager.GetBeachNoise(x, z);
  float fertility = humid; // Derived for now

  float yNormalized = (float)y / (float)config.worldHeight;
  yNormalized = std::max(0.0f, std::min(1.0f, yNormalized));

  uint8_t id = BlockLayerConfig::Get().GetSurfaceBlockId(
      temp, humid, fertility, patch, yNormalized, beach);
  return (BlockType)id;
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

void WorldGenerator::CleanupFloatingIslands(Chunk &chunk) {
  // Post-process to remove floating islands (small clusters of solid blocks
  // entirely surrounded by air/water or nothingness)

  static const int SIZE = CHUNK_SIZE;
  static const int TOTAL_BLOCKS = SIZE * SIZE * SIZE;
  // Local visited array for the chunk (32*32*32 = 32768 bools)
  std::vector<bool> visited(TOTAL_BLOCKS, false);

  // Optimization: Queue for BFS
  std::vector<glm::ivec3> component;
  std::queue<glm::ivec3> q;
  component.reserve(64);

  // Helper lambda for index
  auto getIdx = [](int x, int y, int z) { return (y * SIZE + z) * SIZE + x; };

  for (int y = 0; y < SIZE; ++y) {
    for (int z = 0; z < SIZE; ++z) {
      for (int x = 0; x < SIZE; ++x) {
        int idx = getIdx(x, y, z);
        if (visited[idx])
          continue;

        ChunkBlock cb = chunk.getBlock(x, y, z);
        bool isSolid = (cb.getType() != BlockType::AIR &&
                        cb.getType() != BlockType::WATER);

        if (!isSolid) {
          visited[idx] = true;
          continue;
        }

        // It is Solid and Not Visited.
        // y=0 blocks are intrinsically stable (connected to chunk below)
        bool isStable = (y == 0);
        bool touchesBorder = false;

        // BFS to find component
        component.clear();
        q.push({x, y, z});
        visited[idx] = true;
        component.push_back({x, y, z});

        if (x == 0 || x == SIZE - 1 || z == 0 || z == SIZE - 1)
          touchesBorder = true;

        int count = 0;
        const int MAX_FLOATING_SIZE = 64; // Increased to 64
        bool tooBig = false;

        while (!q.empty()) {
          glm::ivec3 curr = q.front();
          q.pop();
          count++;

          if (count > MAX_FLOATING_SIZE) {
            tooBig = true;
          }

          if (curr.x == 0 || curr.x == SIZE - 1 || curr.z == 0 ||
              curr.z == SIZE - 1) {
            touchesBorder = true;
          }

          static const glm::ivec3 dirs[] = {{0, 1, 0},  {0, -1, 0}, {1, 0, 0},
                                            {-1, 0, 0}, {0, 0, 1},  {0, 0, -1}};

          for (const auto &d : dirs) {
            glm::ivec3 n = curr + d;
            if (n.x >= 0 && n.x < SIZE && n.y >= 0 && n.y < SIZE && n.z >= 0 &&
                n.z < SIZE) {
              int nIdx = getIdx(n.x, n.y, n.z);
              if (!visited[nIdx]) {
                ChunkBlock ncb = chunk.getBlock(n.x, n.y, n.z);
                bool nSolid = (ncb.getType() != BlockType::AIR &&
                               ncb.getType() != BlockType::WATER);
                if (nSolid) {
                  visited[nIdx] = true;
                  q.push(n);
                  if (!tooBig)
                    component.push_back(n);
                }
              }
            }
          }
        }

        if (isStable || touchesBorder || tooBig) {
          // Keep it.
        } else {
          // Delete it.
          for (const auto &p : component) {
            chunk.setBlock(p.x, p.y, p.z, BlockType::AIR);
          }
        }
      }
    }
  }
}
