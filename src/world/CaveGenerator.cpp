#include "CaveGenerator.h"
#include "../debug/Logger.h"
#include "Block.h"
#include "Chunk.h"
#include "ChunkColumn.h"
#include "WorldGenRegion.h"
#include <algorithm>
#include <cmath>
#include <fstream>
#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

// Load CaveConfig from JSON
CaveConfig CaveConfig::LoadFromFile(const std::string &filepath) {
  CaveConfig config;

  try {
    std::ifstream file(filepath);
    if (!file.is_open()) {
      return config; // Return defaults
    }

    json j;
    file >> j;

    // Load main parameters
    if (j.contains("cavesPerChunk"))
      config.cavesPerChunk = j["cavesPerChunk"];
    if (j.contains("chunkRange"))
      config.chunkRange = j["chunkRange"];

    // Tunnel sizes
    if (j.contains("tunnelSizes")) {
      auto ts = j["tunnelSizes"];
      if (ts.contains("horizontalMin"))
        config.horizontalMin = ts["horizontalMin"];
      if (ts.contains("horizontalMax"))
        config.horizontalMax = ts["horizontalMax"];
      if (ts.contains("verticalMin"))
        config.verticalMin = ts["verticalMin"];
      if (ts.contains("verticalMax"))
        config.verticalMax = ts["verticalMax"];
    }

    // Special caves
    if (j.contains("specialCaves")) {
      auto sc = j["specialCaves"];
      if (sc.contains("wideFlatChance"))
        config.wideFlatChance = sc["wideFlatChance"];
      if (sc.contains("tallNarrowChance"))
        config.tallNarrowChance = sc["tallNarrowChance"];
      if (sc.contains("extraBranchyChance"))
        config.extraBranchyChance = sc["extraBranchyChance"];
      if (sc.contains("largeNearLavaChance"))
        config.largeNearLavaChance = sc["largeNearLavaChance"];
    }

    // Curviness
    if (j.contains("curviness")) {
      auto c = j["curviness"];
      if (c.contains("normal"))
        config.curviness_normal = c["normal"];
      if (c.contains("high"))
        config.curviness_high = c["high"];
      if (c.contains("veryLow"))
        config.curviness_veryLow = c["veryLow"];
      if (c.contains("highChance"))
        config.curviness_highChance = c["highChance"];
      if (c.contains("veryLowChance"))
        config.curviness_veryLowChance = c["veryLowChance"];
    }

    // Branching
    if (j.contains("branching")) {
      auto b = j["branching"];
      if (b.contains("horizontalBranchBase"))
        config.horizontalBranchBase = b["horizontalBranchBase"];
      if (b.contains("horizontalBranchExtraBranchy"))
        config.horizontalBranchExtraBranchy = b["horizontalBranchExtraBranchy"];
      if (b.contains("verticalShaftChance"))
        config.verticalShaftChance = b["verticalShaftChance"];
      if (b.contains("maxBranchDepth"))
        config.maxBranchDepth = b["maxBranchDepth"];
      if (b.contains("verticalShaftMinY"))
        config.verticalShaftMinY = b["verticalShaftMinY"];
      if (b.contains("verticalShaftMinRadius"))
        config.verticalShaftMinRadius = b["verticalShaftMinRadius"];
    }

    // Sizing
    if (j.contains("sizing")) {
      auto s = j["sizing"];
      if (s.contains("baseHorizontal"))
        config.baseHorizontal = s["baseHorizontal"];
      if (s.contains("baseVertical"))
        config.baseVertical = s["baseVertical"];
      if (s.contains("minHorizontal"))
        config.minHorizontal = s["minHorizontal"];
      if (s.contains("minVertical"))
        config.minVertical = s["minVertical"];
      if (s.contains("sizeChangeSpeed"))
        config.sizeChangeSpeed = s["sizeChangeSpeed"];
    }

    // Lava
    if (j.contains("lava")) {
      auto l = j["lava"];
      if (l.contains("lavaY"))
        config.lavaY = l["lavaY"];
      if (l.contains("largeCavernMinY"))
        config.largeCavernMinY = l["largeCavernMinY"];
      if (l.contains("largeCavernMaxY"))
        config.largeCavernMaxY = l["largeCavernMaxY"];
      if (l.contains("largeCavernMinRadius"))
        config.largeCavernMinRadius = l["largeCavernMinRadius"];
      if (l.contains("largeCavernMinVertRadius"))
        config.largeCavernMinVertRadius = l["largeCavernMinVertRadius"];
    }

    // Iteration
    if (j.contains("iteration")) {
      auto i = j["iteration"];
      if (i.contains("maxIterationBase"))
        config.maxIterationBase = i["maxIterationBase"];
      if (i.contains("maxIterationVariance"))
        config.maxIterationVariance = i["maxIterationVariance"];
    }

    // Distortion
    if (j.contains("distortion")) {
      auto d = j["distortion"];
      if (d.contains("heightDistortOctaves"))
        config.heightDistortOctaves = d["heightDistortOctaves"];
      if (d.contains("heightDistortFrequency"))
        config.heightDistortFrequency = d["heightDistortFrequency"];
      if (d.contains("heightDistortStrength"))
        config.heightDistortStrength = d["heightDistortStrength"];
    }

    // Hot springs
    if (j.contains("hotSprings")) {
      auto hs = j["hotSprings"];
      if (hs.contains("minY"))
        config.hotSprings_minY = hs["minY"];
      if (hs.contains("maxY"))
        config.hotSprings_maxY = hs["maxY"];
      if (hs.contains("minHorizontalRadius"))
        config.hotSprings_minHorizontalRadius = hs["minHorizontalRadius"];
      if (hs.contains("minVerticalRadius"))
        config.hotSprings_minVerticalRadius = hs["minVerticalRadius"];
      if (hs.contains("minGeologicActivity"))
        config.hotSprings_minGeologicActivity = hs["minGeologicActivity"];
    }

    // Angle variation
    if (j.contains("angleVariation")) {
      auto av = j["angleVariation"];
      if (av.contains("initialVerticalAngleRange"))
        config.initialVerticalAngleRange = av["initialVerticalAngleRange"];
      if (av.contains("verticalAngleDamping"))
        config.verticalAngleDamping = av["verticalAngleDamping"];
      if (av.contains("verticalAngleChangeFactor"))
        config.verticalAngleChangeFactor = av["verticalAngleChangeFactor"];
      if (av.contains("horizontalAngleChangeFactor"))
        config.horizontalAngleChangeFactor = av["horizontalAngleChangeFactor"];
      if (av.contains("majorDirectionChangeChance"))
        config.majorDirectionChangeChance = av["majorDirectionChangeChance"];
      if (av.contains("minorDirectionChangeChance"))
        config.minorDirectionChangeChance = av["minorDirectionChangeChance"];
    }

    // Random events
    if (j.contains("randomEvents")) {
      auto re = j["randomEvents"];
      if (re.contains("goWideChance"))
        config.goWideChance = re["goWideChance"];
      if (re.contains("goThinChance"))
        config.goThinChance = re["goThinChance"];
      if (re.contains("goFlatChance"))
        config.goFlatChance = re["goFlatChance"];
      if (re.contains("goReallyWideChance"))
        config.goReallyWideChance = re["goReallyWideChance"];
      if (re.contains("goReallyTallChance"))
        config.goReallyTallChance = re["goReallyTallChance"];
      if (re.contains("largeLavaCavernChance"))
        config.largeLavaCavernChance = re["largeLavaCavernChance"];
    }

  } catch (const std::exception &e) {
    // Error parsing cave config, use defaults
  }

  return config;
}

CaveGenerator::CaveGenerator(const WorldGenConfig &config)
    : seed(config.seed), worldConfig(config), caveRng(seed + 123128),
      chunkRng(seed) {

  // Load cave configuration
  caveConfig = CaveConfig::LoadFromFile("assets/worldgen/caves.json");

  // Initialize height distortion noise
  auto fnSimplex = FastNoise::New<FastNoise::Simplex>();
  auto fnFractal = FastNoise::New<FastNoise::FractalFBm>();
  fnFractal->SetSource(fnSimplex);
  fnFractal->SetOctaveCount(caveConfig.heightDistortOctaves);
  heightDistortNoise = fnFractal;
}

void CaveGenerator::GenerateHeightDistortion(ChunkColumn &column, int cx,
                                             int cz) {
  // Generate 2D noise for cave height distortion
  static thread_local std::vector<float> distortMap(CHUNK_SIZE * CHUNK_SIZE);

  auto metadata = heightDistortNoise->GenUniformGrid2D(
      distortMap.data(), cx * CHUNK_SIZE, cz * CHUNK_SIZE, CHUNK_SIZE,
      CHUNK_SIZE, caveConfig.heightDistortFrequency, seed + 12);

  // Store in column (need to add this field to ChunkColumn)
  for (int z = 0; z < CHUNK_SIZE; z++) {
    for (int x = 0; x < CHUNK_SIZE; x++) {
      int idx = z * CHUNK_SIZE + x;
      // Normalize to range [0, 255] with center at 127
      float val = distortMap[idx] - 0.5f;
      val = val > 0 ? std::max(0.0f, val - 0.07f) : std::min(0.0f, val + 0.07f);
      column.caveHeightDistort[idx] =
          static_cast<uint8_t>(128.0f * val + 127.0f);
    }
  }
}

void CaveGenerator::InitChunkRng(int chunkX, int chunkZ) {
  // Initialize chunk RNG with position-based seed
  uint32_t chunkSeed = seed ^ (chunkX * 1619) ^ (chunkZ * 31337);
  chunkRng.seed(chunkSeed);
}

void CaveGenerator::GenerateCaves(WorldGenRegion &region, int chunkX,
                                  int chunkZ) {
  // Only spawn caves that ORIGINATE from the current chunk
  // (they can still CARVE into neighboring chunks via WorldGenRegion)
  InitChunkRng(chunkX, chunkZ);

  // Determine how many caves spawn in this chunk
  // Integer part = guaranteed caves, fractional part = probability of one more
  int quantityCaves = static_cast<int>(caveConfig.cavesPerChunk);
  float fractional = caveConfig.cavesPerChunk - quantityCaves;
  float rand =
      static_cast<float>(chunkRng()) / static_cast<float>(chunkRng.max());
  if (rand < fractional) {
    quantityCaves++;
  }

  while (quantityCaves-- > 0) {
    // Random starting position within THIS chunk (in WORLD coordinates)
    int rndSize = CHUNK_SIZE * CHUNK_SIZE * (worldConfig.worldHeight - 20);
    int rnd = RandomInt(rndSize);

    // Calculate world coordinates within the current chunk
    int posX = chunkX * CHUNK_SIZE + (rnd % CHUNK_SIZE);
    rnd /= CHUNK_SIZE;
    int posZ = chunkZ * CHUNK_SIZE + (rnd % CHUNK_SIZE);
    rnd /= CHUNK_SIZE;
    int posY = rnd + 8;

    // DEBUG: Log cave starting position (Removed)
    // LOG_WORLD_INFO("  Cave start: world pos ({}, {}, {})", posX, posY, posZ);

    // Initial angles
    float horAngle = RandomFloat(0.0f, 2.0f * glm::pi<float>());
    float vertAngle =
        (RandomFloat(0.0f, 1.0f) - 0.5f) * caveConfig.initialVerticalAngleRange;

    // Initial sizes
    float horizontalSize =
        RandomFloat(caveConfig.horizontalMin, caveConfig.horizontalMax);
    float verticalSize =
        RandomFloat(caveConfig.verticalMin, caveConfig.verticalMax);

    // Determine special types
    rnd = RandomInt(100000);

    bool extraBranchy = (posY < worldConfig.seaLevel / 2) && ((rnd % 50) == 0);
    bool largeNearLava = ((rnd % 10) < 3);

    // Wide/flat cave
    if ((rnd % 100) < static_cast<int>(caveConfig.wideFlatChance * 100.0f)) {
      horizontalSize = RandomFloat(caveConfig.horizontalMin,
                                   caveConfig.horizontalMax + 1.0f);
      verticalSize = 0.25f + RandomFloat(0.0f, 0.2f);
    }
    // Tall/narrow cave
    else if ((rnd % 100) == 4) {
      horizontalSize = 0.75f + RandomFloat(0.0f, 1.0f);
      verticalSize =
          RandomFloat(caveConfig.verticalMin, caveConfig.verticalMax) * 2.0f;
    }

    // Determine curviness
    float curviness = caveConfig.curviness_normal;
    rnd = rnd / 100;
    if ((rnd % 100) == 0) {
      curviness = caveConfig.curviness_veryLow;
    } else if ((rnd % 1000) < 30) {
      curviness = caveConfig.curviness_high;
    }

    // Max iterations
    int maxIterations = caveConfig.chunkRange * CHUNK_SIZE - CHUNK_SIZE / 2;
    maxIterations = maxIterations - RandomInt(maxIterations / 4);

    // Re-seed cave RNG for this specific cave
    int caveSeed = RandomInt(10000000);
    caveRng.seed(seed + caveSeed);

    // Start the tunnel
    CarveTunnel(region, chunkX, chunkZ, posX, posY, posZ, horAngle, vertAngle,
                horizontalSize, verticalSize, 0, maxIterations, 0, extraBranchy,
                curviness, largeNearLava);
  }
}

void CaveGenerator::CarveTunnel(WorldGenRegion &region, int chunkX, int chunkZ,
                                double posX, double posY, double posZ,
                                float horAngle, float vertAngle,
                                float horizontalSize, float verticalSize,
                                int currentIteration, int maxIterations,
                                int branchLevel, bool extraBranchy,
                                float curviness, bool largeNearLava) {

  // Accumulated gain/loss for dynamic sizing
  float horRadiusGain = 0.0f;
  float horRadiusLoss = 0.0f;
  float horRadiusGainAccum = 0.0f;
  float horRadiusLossAccum = 0.0f;

  float verHeightGain = 0.0f;
  float verHeightLoss = 0.0f;
  float verHeightGainAccum = 0.0f;
  float verHeightLossAccum = 0.0f;

  float horAngleChange = 0.0f;
  float vertAngleChange = 0.0f;

  float sizeChangeSpeedAccum = caveConfig.sizeChangeSpeed;
  float sizeChangeSpeedGain = 0.0f;

  int branchRand = (branchLevel + 1) *
                   (extraBranchy ? caveConfig.horizontalBranchExtraBranchy
                                 : caveConfig.horizontalBranchBase);

  while (currentIteration++ < maxIterations) {
    float relPos = static_cast<float>(currentIteration) / maxIterations;

    // Calculate radii using sine wave
    float horRadius = caveConfig.baseHorizontal +
                      std::sin(relPos * glm::pi<float>()) * horizontalSize +
                      horRadiusGainAccum;
    horRadius = std::min(horRadius, std::max(caveConfig.minHorizontal,
                                             horRadius - horRadiusLossAccum));

    float vertRadius = caveConfig.baseVertical +
                       std::sin(relPos * glm::pi<float>()) *
                           (verticalSize + horRadiusLossAccum / 4.0f) +
                       verHeightGainAccum;
    vertRadius =
        std::min(vertRadius, std::max(caveConfig.minVertical,
                                      vertRadius - verHeightLossAccum));

    // Movement vectors
    float advanceHor = std::cos(vertAngle);
    float advanceVer = std::sin(vertAngle);

    // Caves get bigger near lava layer
    if (largeNearLava) {
      float factor =
          1.0f + std::max(0.0f, 1.0f - static_cast<float>(
                                           std::abs(posY - caveConfig.lavaY)) /
                                           10.0f);
      horRadius *= factor;
      vertRadius *= factor;
    }

    if (vertRadius < 1.0f)
      vertAngle *= 0.1f;

    // Advance position
    posX += std::cos(horAngle) * advanceHor;
    posY += glm::clamp(advanceVer, -vertRadius, vertRadius);
    posZ += std::sin(horAngle) * advanceHor;

    vertAngle *= caveConfig.verticalAngleDamping;

    // Random variations
    int rrnd = RandomInt(800000);
    if (rrnd / 10000 == 0) {
      sizeChangeSpeedGain =
          (RandomFloat(0.0f, 1.0f) * RandomFloat(0.0f, 1.0f)) / 2.0f;
    }

    bool genHotSpring = false;
    int rnd = rrnd % 10000;

    // Random direction/size changes (using probabilities from config)
    if ((rnd -= 30) <= 0) {
      // Major direction change
      horAngle = RandomFloat(0.0f, glm::two_pi<float>());
    } else if ((rnd -= 76) <= 0) {
      // Minor direction change
      horAngle += RandomFloat(0.0f, glm::pi<float>()) - glm::half_pi<float>();
    } else if ((rnd -= 60) <= 0) {
      // Go wide
      horRadiusGain = RandomFloat(0.0f, 1.0f) * RandomFloat(0.0f, 1.0f) * 3.5f;
    } else if ((rnd -= 60) <= 0) {
      // Go thin
      horRadiusLoss = RandomFloat(0.0f, 1.0f) * RandomFloat(0.0f, 1.0f) * 10.0f;
    } else if ((rnd -= 50) <= 0) {
      // Go flat (below sea level only)
      if (posY < worldConfig.seaLevel - 10) {
        verHeightLoss =
            RandomFloat(0.0f, 1.0f) * RandomFloat(0.0f, 1.0f) * 12.0f;
        horRadiusGain =
            std::max(horRadiusGain,
                     RandomFloat(0.0f, 1.0f) * RandomFloat(0.0f, 1.0f) * 3.0f);
      }
    } else if ((rnd -= 9) <= 0) {
      // Go really wide (deep only)
      if (posY < worldConfig.seaLevel - 20) {
        horRadiusGain =
            1.0f + RandomFloat(0.0f, 1.0f) * RandomFloat(0.0f, 1.0f) * 5.0f;
      }
    } else if ((rnd -= 9) <= 0) {
      // Go really tall
      verHeightGain =
          2.0f + RandomFloat(0.0f, 1.0f) * RandomFloat(0.0f, 1.0f) * 7.0f;
    } else if ((rnd -= 100) <= 0) {
      // Large lava caverns (very deep only)
      if (posY < caveConfig.largeCavernMinY) {
        verHeightGain =
            2.0f + RandomFloat(0.0f, 1.0f) * RandomFloat(0.0f, 1.0f) * 5.0f;
        horRadiusGain =
            4.0f + RandomFloat(0.0f, 1.0f) * RandomFloat(0.0f, 1.0f) * 9.0f;
      }
    }

    // Check for hot spring conditions
    if (posY > caveConfig.hotSprings_maxY &&
        posY < caveConfig.hotSprings_minY &&
        horRadius > caveConfig.hotSprings_minHorizontalRadius &&
        vertRadius > caveConfig.hotSprings_minVerticalRadius) {
      genHotSpring = true;
    }

    // Update accumulations
    sizeChangeSpeedAccum =
        std::max(0.1f, sizeChangeSpeedAccum + sizeChangeSpeedGain * 0.05f);
    sizeChangeSpeedGain -= 0.02f;

    horRadiusGainAccum = std::max(
        0.0f, horRadiusGainAccum + horRadiusGain * sizeChangeSpeedAccum);
    horRadiusGain -= 0.45f;

    horRadiusLossAccum = std::max(
        0.0f, horRadiusLossAccum + horRadiusLoss * sizeChangeSpeedAccum);
    horRadiusLoss -= 0.4f;

    verHeightGainAccum = std::max(
        0.0f, verHeightGainAccum + verHeightGain * sizeChangeSpeedAccum);
    verHeightGain -= 0.45f;

    verHeightLossAccum = std::max(
        0.0f, verHeightLossAccum + verHeightLoss * sizeChangeSpeedAccum);
    verHeightLoss -= 0.4f;

    // Update angles
    horAngle += curviness * horAngleChange;
    vertAngle += curviness * vertAngleChange;

    vertAngleChange = 0.9f * vertAngleChange +
                      (RandomFloat(-1.0f, 1.0f) * RandomFloat(0.0f, 1.0f) *
                       caveConfig.verticalAngleChangeFactor);
    horAngleChange = 0.9f * horAngleChange +
                     (RandomFloat(-1.0f, 1.0f) * RandomFloat(0.0f, 1.0f) *
                      caveConfig.horizontalAngleChangeFactor);

    if (rrnd % 140 == 0) {
      horAngleChange *= RandomFloat(0.0f, 1.0f) * 6.0f;
    }

    // Horizontal branching
    int brand = branchRand + 2 * std::max(0, static_cast<int>(posY) -
                                                 (worldConfig.seaLevel - 20));
    if (branchLevel < caveConfig.maxBranchDepth &&
        (vertRadius > 1.0f || horRadius > 1.0f) && RandomInt(brand) == 0) {
      CarveTunnel(
          region, chunkX, chunkZ, posX, posY + verHeightGainAccum / 2.0, posZ,
          horAngle +
              (RandomFloat(0.0f, 1.0f) + RandomFloat(0.0f, 1.0f) - 1.0f) +
              glm::pi<float>(),
          vertAngle + (RandomFloat(0.0f, 1.0f) - 0.5f) *
                          (RandomFloat(0.0f, 1.0f) - 0.5f),
          horizontalSize, verticalSize + verHeightGainAccum, currentIteration,
          maxIterations - RandomInt(maxIterations / 2), branchLevel + 1,
          extraBranchy, curviness, largeNearLava);
    }

    // Vertical shaft branching
    if (branchLevel < 1 && horRadius > caveConfig.verticalShaftMinRadius &&
        posY > caveConfig.verticalShaftMinY &&
        RandomInt(caveConfig.verticalShaftChance) == 0) {
      CarveShaft(
          region, chunkX, chunkZ, posX, posY + verHeightGainAccum / 2.0, posZ,
          horAngle +
              (RandomFloat(0.0f, 1.0f) + RandomFloat(0.0f, 1.0f) - 1.0f) +
              glm::pi<float>(),
          -glm::half_pi<float>() - 0.1f + 0.2f * RandomFloat(0.0f, 1.0f),
          std::min(3.5f, horRadius - 1.0f), verticalSize + verHeightGainAccum,
          currentIteration,
          maxIterations - RandomInt(maxIterations / 2) +
              static_cast<int>((posY / 5.0) *
                               (0.5f + 0.5f * RandomFloat(0.0f, 1.0f))),
          branchLevel);
      branchLevel++;
    }

    // Skip carving if large and every 5th iteration (optimization)
    if (horRadius >= 2.0f && rrnd % 5 == 0)
      continue;

    // Check if we're within the target chunk region - REMOVED legacy check
    // The WorldGenRegion handles bounds checking safely

    // Carve the blocks
    SetBlocks(region, horRadius, vertRadius + verHeightGainAccum, posX,
              posY + verHeightGainAccum / 2.0, posZ, chunkX, chunkZ,
              genHotSpring);
  }
}

void CaveGenerator::CarveShaft(WorldGenRegion &region, int chunkX, int chunkZ,
                               double posX, double posY, double posZ,
                               float horAngle, float vertAngle,
                               float horizontalSize, float verticalSize,
                               int caveCurrentIteration, int maxIterations,
                               int branchLevel) {

  float vertAngleChange = 0.0f;
  int currentIteration = 0;

  while (currentIteration++ < maxIterations) {
    float relPos = static_cast<float>(currentIteration) / maxIterations;

    float horRadius = horizontalSize * (1.0f - relPos * 0.33f);
    float vertRadius = horRadius * verticalSize;

    float advanceHor = std::cos(vertAngle);
    float advanceVer = std::sin(vertAngle);

    if (vertRadius < 1.0f)
      vertAngle *= 0.1f;

    posX += std::cos(horAngle) * advanceHor;
    posY += glm::clamp(advanceVer, -vertRadius, vertRadius);
    posZ += std::sin(horAngle) * advanceHor;

    vertAngle += 0.1f * vertAngleChange;
    vertAngleChange = 0.9f * vertAngleChange +
                      (RandomFloat(0.0f, 1.0f) - RandomFloat(0.0f, 1.0f)) *
                          RandomFloat(0.0f, 1.0f) / 3.0f;

    // Horizontal branches at end of shaft
    if (maxIterations - currentIteration < 10) {
      int num = 3 + RandomInt(4);
      for (int i = 0; i < num; i++) {
        CarveTunnel(region, chunkX, chunkZ, posX, posY, posZ,
                    RandomFloat(0.0f, 2.0f * glm::pi<float>()),
                    (RandomFloat(0.0f, 1.0f) - 0.5f) * 0.25f,
                    horizontalSize + 1.0f, verticalSize, caveCurrentIteration,
                    maxIterations, 1, false, caveConfig.curviness_normal,
                    false);
      }
      return;
    }

    if (RandomInt(5) == 0 && horRadius >= 2.0f)
      continue;

    // Check bounds - REMOVED legacy check
    // if (posX <= -horRadius * 2.0 || posX >= CHUNK_SIZE + horRadius * 2.0 ||
    //     posZ <= -horRadius * 2.0 || posZ >= CHUNK_SIZE + horRadius * 2.0)
    //   continue;

    SetBlocks(region, horRadius, vertRadius, posX, posY, posZ, chunkX, chunkZ,
              false);
  }
}

bool CaveGenerator::SetBlocks(WorldGenRegion &region, float horRadius,
                              float vertRadius, double centerX, double centerY,
                              double centerZ, int chunkX, int chunkZ,
                              bool genHotSpring) {

  // Expand radius for water check
  float checkHorRadius = horRadius + 1.0f;
  float checkVertRadius = vertRadius + 2.0f;

  // Use world coordinates, not chunk-local
  int mindx = static_cast<int>(centerX - checkHorRadius);
  int maxdx = static_cast<int>(centerX + checkHorRadius + 1.0);
  int mindy = static_cast<int>(
      glm::clamp(centerY - checkVertRadius * 0.7, 1.0,
                 static_cast<double>(worldConfig.worldHeight - 1)));
  int maxdy = static_cast<int>(
      glm::clamp(centerY + checkVertRadius + 1.0, 1.0,
                 static_cast<double>(worldConfig.worldHeight - 1)));
  int mindz = static_cast<int>(centerZ - checkHorRadius);
  int maxdz = static_cast<int>(centerZ + checkHorRadius + 1.0);

  // First pass: check for water
  double hRadiusSq = checkHorRadius * checkHorRadius;
  double vRadiusSq = checkVertRadius * checkVertRadius;

  Block *waterBlock = BlockRegistry::getInstance().getBlock(BlockType::WATER);
  Block *iceBlock = BlockRegistry::getInstance().getBlock(BlockType::ICE);

  for (int lx = mindx; lx <= maxdx; lx++) {
    double xdistRel = (lx - centerX) * (lx - centerX) / hRadiusSq;
    for (int lz = mindz; lz <= maxdz; lz++) {
      double zdistRel = (lz - centerZ) * (lz - centerZ) / hRadiusSq;

      for (int y = mindy; y <= maxdy + 10; y++) {
        if (y > worldConfig.worldHeight - 1)
          continue;

        double yDist = y - centerY;
        double ydistRel = yDist * yDist / vRadiusSq;

        if (xdistRel + ydistRel + zdistRel > 1.0)
          continue;

        // Check for water/ice
        Block *block = region.getBlockPtr(lx, y, lz);
        if (block == waterBlock || block == iceBlock) {
          return false; // Abort carving
        }
      }
    }
  }

  // Second pass: actually carve
  hRadiusSq = horRadius * horRadius;
  vRadiusSq = vertRadius * vertRadius;

  mindx = static_cast<int>(centerX - horRadius);
  maxdx = static_cast<int>(centerX + horRadius + 1.0);
  mindy = static_cast<int>(
      glm::clamp(centerY - vertRadius * 0.7, 1.0,
                 static_cast<double>(worldConfig.worldHeight - 1)));
  maxdy = static_cast<int>(
      glm::clamp(centerY + vertRadius + 1.0, 1.0,
                 static_cast<double>(worldConfig.worldHeight - 1)));
  mindz = static_cast<int>(centerZ - horRadius);
  maxdz = static_cast<int>(centerZ + horRadius + 1.0);

  Block *airBlock = BlockRegistry::getInstance().getBlock(BlockType::AIR);
  Block *lavaBlock = BlockRegistry::getInstance().getBlock(BlockType::LAVA);

  for (int lx = mindx; lx <= maxdx; lx++) {
    double xdistRel = (lx - centerX) * (lx - centerX) / hRadiusSq;

    for (int lz = mindz; lz <= maxdz; lz++) {
      double zdistRel = (lz - centerZ) * (lz - centerZ) / hRadiusSq;

      for (int y = maxdy + 10; y >= mindy; y--) {
        if (y > worldConfig.worldHeight - 1)
          continue;

        double yDist = y - centerY;
        double ydistRel = yDist * yDist / vRadiusSq;

        if (xdistRel + ydistRel + zdistRel > 1.0)
          continue;

        // Carve the block
        if (y < caveConfig.lavaY) {
          region.setBlock(lx, y, lz, lavaBlock);
        } else {
          region.setBlock(lx, y, lz, airBlock);
        }
      }
    }
  }

  return true;
}

float CaveGenerator::RandomFloat(float min, float max) {
  std::uniform_real_distribution<float> dist(min, max);
  return dist(chunkRng); // Use chunkRng for per-chunk variation
}

int CaveGenerator::RandomInt(int max) {
  if (max <= 0)
    return 0;
  std::uniform_int_distribution<int> dist(0, max - 1);
  return dist(chunkRng); // Use chunkRng for per-chunk variation
}
