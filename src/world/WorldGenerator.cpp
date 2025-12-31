#include "WorldGenerator.h"
#include "../debug/Logger.h"
#include "../debug/Profiler.h"
#include "Block.h"
#include "Chunk.h"
#include <glm/glm.hpp>
#include <glm/gtc/noise.hpp>

#include "FloraDecorator.h"
#include "OreDecorator.h"
#include "TreeDecorator.h"

WorldGenerator::WorldGenerator(const WorldGenConfig &config)
    : config(config), seed(config.seed) {
  if (config.enableOre)
    decorators.push_back(new OreDecorator());
  if (config.enableTrees)
    decorators.push_back(new TreeDecorator());
  if (config.enableFlora)
    decorators.push_back(new FloraDecorator());

  // Initialize landform presets
  InitializeLandforms();

  // Initialize cave generator
  caveGenerator = new CaveGenerator(config);
}

WorldGenerator::~WorldGenerator() {
  for (auto d : decorators)
    delete d;
  decorators.clear();

  delete caveGenerator;
}

int WorldGenerator::ComputeHeight(int x, int z) {
  // Get landform noise for blending
  float landformNoise = GetLandformNoise(x, z);

  int seedX = (seed * 1337) % 65536;
  int seedZ = (seed * 9999) % 65536;
  float nx = (float)x + (float)seedX;
  float nz = (float)z + (float)seedZ;

  // Enhanced octave system with 10 octaves
  const int numOctaves = 10;
  float noiseHeight = 0.0f;
  float frequency = config.terrainScale;

  // Blend between landforms for smooth transitions
  LandformConfig *primary = nullptr;
  LandformConfig *secondary = nullptr;
  float blendFactor = 0.0f;

  if (landformNoise < -0.4f) {
    primary = &landforms["oceans"];
    secondary = &landforms["valleys"];
    blendFactor = (landformNoise + 0.6f) / 0.2f; // -0.6 to -0.4
  } else if (landformNoise < 0.0f) {
    primary = &landforms["valleys"];
    secondary = &landforms["plains"];
    blendFactor = (landformNoise + 0.4f) / 0.4f; // -0.4 to 0.0
  } else if (landformNoise < 0.4f) {
    primary = &landforms["plains"];
    secondary = &landforms["hills"];
    blendFactor = (landformNoise - 0.0f) / 0.4f; // 0.0 to 0.4
  } else {
    primary = &landforms["hills"];
    secondary = &landforms["mountains"];
    blendFactor = (landformNoise - 0.4f) / 0.4f; // 0.4 to 0.8
  }

  blendFactor = std::max(0.0f, std::min(1.0f, blendFactor));

  for (int i = 0; i < numOctaves; ++i) {
    float octaveValue = glm::perlin(glm::vec2(nx, nz) * frequency);

    // Blend octave amplitudes between landforms
    float ampP = (i < primary->octaveAmplitudes.size())
                     ? primary->octaveAmplitudes[i]
                     : primary->octaveAmplitudes[i]; // Default to landform base
    float ampS = (i < secondary->octaveAmplitudes.size())
                     ? secondary->octaveAmplitudes[i]
                     : secondary->octaveAmplitudes[i];

    // Check for overrides in config
    auto itP = config.landformOverrides.find(primary->name);
    if (itP != config.landformOverrides.end() &&
        i < itP->second.octaveAmplitudes.size()) {
      ampP = itP->second.octaveAmplitudes[i];
    }
    auto itS = config.landformOverrides.find(secondary->name);
    if (itS != config.landformOverrides.end() &&
        i < itS->second.octaveAmplitudes.size()) {
      ampS = itS->second.octaveAmplitudes[i];
    }

    float blendedAmplitude = ampP * (1.0f - blendFactor) + ampS * blendFactor;

    float blendedThreshold =
        primary->octaveThresholds[i] * (1.0f - blendFactor) +
        secondary->octaveThresholds[i] * blendFactor;

    if (octaveValue > blendedThreshold) {
      noiseHeight += (octaveValue - blendedThreshold) * blendedAmplitude;
    }

    frequency *= 2.0f;
  }

  // Normalize by sum of blended amplitudes
  float maxPossibleAmplitude = 0.0f;
  for (int i = 0; i < numOctaves; ++i) {
    float blendedAmp = primary->octaveAmplitudes[i] * (1.0f - blendFactor) +
                       secondary->octaveAmplitudes[i] * blendFactor;
    maxPossibleAmplitude += blendedAmp;
  }
  if (maxPossibleAmplitude > 0.0f) {
    noiseHeight /= maxPossibleAmplitude;
  }

  // Blend base height and variation
  float blendedBaseHeight = primary->baseHeight * (1.0f - blendFactor) +
                            secondary->baseHeight * blendFactor;
  float blendedVariation = primary->heightVariation * (1.0f - blendFactor) +
                           secondary->heightVariation * blendFactor;

  int baseFinalHeight =
      (int)(blendedBaseHeight + noiseHeight * blendedVariation);

  // River Carving
  if (config.enableRivers) {
    float carveFactor = GetRiverCarveFactor(x, z);
    if (carveFactor > 0.0f) {
      // Ensure it reaches sea level + some channel depth
      float heightToSea = (float)(baseFinalHeight - config.seaLevel);
      float dynamicDepth = std::max(config.riverDepth, heightToSea + 2.0f);
      baseFinalHeight -= (int)(dynamicDepth * carveFactor);
    }
  }

  return baseFinalHeight;
}

float WorldGenerator::GetRiverCarveFactor(int x, int z) {
  if (!config.enableRivers)
    return 0.0f;

  int seedR = (seed * 1234) % 65536;
  float rx = (float)x + (float)seedR;
  float rz = (float)z + (float)seedR;

  float riverNoise = glm::perlin(glm::vec2(rx, rz) * config.riverScale);
  float riverVal = std::abs(riverNoise);

  if (riverVal < config.riverThreshold) {
    float t = riverVal / config.riverThreshold;
    return 1.0f - (t * t);
  }
  return 0.0f;
}

int WorldGenerator::GetHeightForLandform(const std::string &name, int x,
                                         int z) {
  auto it = landforms.find(name);
  if (it == landforms.end()) {
    return config.seaLevel; // Default if landform not found
  }

  LandformConfig *landform = &it->second;

  int seedX = (seed * 1337) % 65536;
  int seedZ = (seed * 9999) % 65536;
  float nx = (float)x + (float)seedX;
  float nz = (float)z + (float)seedZ;

  const int numOctaves = 10;
  float noiseHeight = 0.0f;
  float frequency = config.terrainScale;

  for (int i = 0; i < numOctaves; ++i) {
    float octaveValue = glm::perlin(glm::vec2(nx, nz) * frequency);

    float amp = (i < landform->octaveAmplitudes.size())
                    ? landform->octaveAmplitudes[i]
                    : 0.0f;

    // Check for overrides in config
    auto itOverride = config.landformOverrides.find(name);
    if (itOverride != config.landformOverrides.end() &&
        i < itOverride->second.octaveAmplitudes.size()) {
      amp = itOverride->second.octaveAmplitudes[i];
    }

    float threshold = (i < landform->octaveThresholds.size())
                          ? landform->octaveThresholds[i]
                          : 0.0f;

    if (octaveValue > threshold) {
      noiseHeight += (octaveValue - threshold) * amp;
    }

    frequency *= 2.0f;
  }

  // Normalize
  float maxPossibleAmplitude = 0.0f;
  for (int i = 0; i < numOctaves; ++i) {
    if (i < landform->octaveAmplitudes.size()) {
      maxPossibleAmplitude += landform->octaveAmplitudes[i];
    }
  }
  if (maxPossibleAmplitude > 0.0f) {
    noiseHeight /= maxPossibleAmplitude;
  }

  int finalHeight =
      (int)(landform->baseHeight + noiseHeight * landform->heightVariation);

  // Apply river carving
  if (config.enableRivers) {
    float carveFactor = GetRiverCarveFactor(x, z);
    if (carveFactor > 0.0f) {
      float heightToSea = (float)(finalHeight - config.seaLevel);
      float dynamicDepth = std::max(config.riverDepth, heightToSea + 2.0f);
      finalHeight -= (int)(dynamicDepth * carveFactor);
    }
  }

  return finalHeight;
}

// Ensure GenerateFixedMaps is defined
void WorldGenerator::GenerateFixedMaps() {
  if (!config.fixedWorld)
    return;

  int size = config.fixedWorldSize;
  fixedHeightMap.resize(size * size);
  fixedTempMap.resize(size * size);
  fixedHumidMap.resize(size * size);
  fixedBiomeMap.resize(size * size);

  int halfSize = size / 2;
  for (int z = 0; z < size; ++z) {
    for (int x = 0; x < size; ++x) {
      int idx = x + z * size;
      // Convert index to world coordinates (centered)
      int worldX = x - halfSize;
      int worldZ = z - halfSize;

      fixedHeightMap[idx] = ComputeHeight(worldX, worldZ);
      fixedTempMap[idx] = ComputeTemperature(worldX, worldZ, -1);
      fixedHumidMap[idx] = ComputeHumidity(worldX, worldZ);
      fixedBiomeMap[idx] = ComputeBiome(worldX, worldZ, -1);
    }
  }
}

int WorldGenerator::GetHeight(int x, int z) {
  if (config.fixedWorld && !fixedHeightMap.empty()) {
    int halfSize = config.fixedWorldSize / 2;
    int idxX = x + halfSize;
    int idxZ = z + halfSize;

    if (idxX >= 0 && idxX < config.fixedWorldSize && idxZ >= 0 &&
        idxZ < config.fixedWorldSize) {
      return fixedHeightMap[idxX + idxZ * config.fixedWorldSize];
    }
    return 60; // Default ocean level outside bounds
  }
  return ComputeHeight(x, z);
}

void WorldGenerator::GetLandformBlend(int x, int z, std::string &primary,
                                      std::string &secondary,
                                      float &blendFactor) {
  float landformNoise = GetLandformNoise(x, z);

  if (landformNoise < -0.4f) {
    primary = "oceans";
    secondary = "valleys";
    blendFactor = (landformNoise + 0.6f) / 0.2f;
  } else if (landformNoise < 0.0f) {
    primary = "valleys";
    secondary = "plains";
    blendFactor = (landformNoise + 0.4f) / 0.4f;
  } else if (landformNoise < 0.4f) {
    primary = "plains";
    secondary = "hills";
    blendFactor = (landformNoise - 0.0f) / 0.4f;
  } else {
    primary = "hills";
    secondary = "mountains";
    blendFactor = (landformNoise - 0.4f) / 0.4f;
  }

  blendFactor = std::max(0.0f, std::min(1.0f, blendFactor));
}

float WorldGenerator::ComputeTemperature(int x, int z, int y) {

  // Very low frequency noise for large biomes
  int seedT = (seed * 555) % 65536;
  float nx = (float)x + (float)seedT;
  float nz = (float)z + (float)seedT;
  // Scale 0.001f means biomes are ~1000 blocks wide
  float temp = glm::perlin(glm::vec2(nx, nz) * config.tempScale);

  // Apply altitude-based temperature decrease (lapse rate)
  if (y != -1 && config.temperatureLapseRate > 0.0f && y > config.seaLevel) {
    float altitudeAboveSeaLevel = (float)(y - config.seaLevel);
    temp -= altitudeAboveSeaLevel * config.temperatureLapseRate;
  }
  // Apply depth-based temperature increase (geothermal gradient)
  else if (y != -1 && config.geothermalGradient > 0.0f && y < config.seaLevel) {
    float depthBelowSeaLevel = (float)(config.seaLevel - y);
    temp += depthBelowSeaLevel * config.geothermalGradient;
  }

  return temp;
}

float WorldGenerator::GetTemperature(int x, int z, int y) {
  if (config.fixedWorld && !fixedTempMap.empty()) {
    int halfSize = config.fixedWorldSize / 2;
    int idxX = x + halfSize;
    int idxZ = z + halfSize;

    if (idxX >= 0 && idxX < config.fixedWorldSize && idxZ >= 0 &&
        idxZ < config.fixedWorldSize) {
      float temp = fixedTempMap[idxX + idxZ * config.fixedWorldSize];
      // Still need to apply lapse rate dynamically because y changes
      if (y != -1 && config.temperatureLapseRate > 0.0f &&
          y > config.seaLevel) {
        float altitudeAboveSeaLevel = (float)(y - config.seaLevel);
        temp -= altitudeAboveSeaLevel * config.temperatureLapseRate;
      } else if (y != -1 && config.geothermalGradient > 0.0f &&
                 y < config.seaLevel) {
        float depthBelowSeaLevel = (float)(config.seaLevel - y);
        temp += depthBelowSeaLevel * config.geothermalGradient;
      }
      return temp;
    }
    return 0.5f; // Failure default
  }
  return ComputeTemperature(x, z, y);
}

float WorldGenerator::ComputeHumidity(int x, int z) {

  int seedH = (seed * 888) % 65536;
  float nx = (float)x + (float)seedH;
  float nz = (float)z + (float)seedH;
  return glm::perlin(glm::vec2(nx, nz) * config.humidityScale);
}

float WorldGenerator::GetHumidity(int x, int z) {
  if (config.fixedWorld && !fixedHumidMap.empty()) {
    int halfSize = config.fixedWorldSize / 2;
    int idxX = x + halfSize;
    int idxZ = z + halfSize;

    if (idxX >= 0 && idxX < config.fixedWorldSize && idxZ >= 0 &&
        idxZ < config.fixedWorldSize) {
      return fixedHumidMap[idxX + idxZ * config.fixedWorldSize];
    }
    return 0.5f;
  }
  return ComputeHumidity(x, z);
}

Biome WorldGenerator::ComputeBiome(int x, int z, int y) {
  float temp = ComputeTemperature(
      x, z, y); // Use Compute here to avoid double lookup if not needed
  float humidity = ComputeHumidity(x, z); // Use Compute here

  // Add high-frequency variation noise to break up smooth blobs
  if (config.biomeVariation > 0.0f) {
    int seedV = (seed * 5555) % 65536;
    float vx = (float)x + (float)seedV;
    float vz = (float)z + (float)seedV;

    // Use much higher frequency noise for variation (0.05 instead of 0.02)
    float varNoise1 = glm::perlin(glm::vec2(vx, vz) * 0.05f);
    float varNoise2 = glm::perlin(glm::vec2(vx * 1.3f, vz * 1.3f) * 0.08f);

    // Combine two noise layers for more complex patterns
    float combinedNoise = (varNoise1 + varNoise2 * 0.5f) / 1.5f;

    // Apply much stronger variation - multiply by 2 to make it very noticeable
    temp += combinedNoise * config.biomeVariation * 2.0f;
    humidity += combinedNoise * config.biomeVariation * 1.6f;
  }

  // Normalize logic slightly if needed, but perlin is approx -1 to 1

  if (temp > 0.3f) {
    // Hot
    if (humidity < -0.2f)
      return BIOME_DESERT; // Hot and Dry
    return BIOME_FOREST;   // Hot and Wet (Jungle-ish)
  } else if (temp < -0.3f) {
    // Cold - significantly cold becomes Tundra
    return BIOME_TUNDRA;
  }

  // Moderate Temp
  if (humidity < -0.3f)
    return BIOME_PLAINS;
  return BIOME_FOREST;
}

Biome WorldGenerator::GetBiome(int x, int z, int y) {
  if (config.fixedWorld && !fixedTempMap.empty() && !fixedHumidMap.empty()) {
    int halfSize = config.fixedWorldSize / 2;
    int idxX = x + halfSize;
    int idxZ = z + halfSize;

    if (idxX >= 0 && idxX < config.fixedWorldSize && idxZ >= 0 &&
        idxZ < config.fixedWorldSize) {
      // Fetch cached data
      float temp = fixedTempMap[idxX + idxZ * config.fixedWorldSize];
      float humidity = fixedHumidMap[idxX + idxZ * config.fixedWorldSize];

      // Apply lapse rate again (logic duplicated from GetTemp)
      if (y != -1 && config.temperatureLapseRate > 0.0f &&
          y > config.seaLevel) {
        float altitudeAboveSeaLevel = (float)(y - config.seaLevel);
        temp -= altitudeAboveSeaLevel * config.temperatureLapseRate;
      }

      // Normalize logic (same as ComputeBiome)
      if (temp > 0.3f) {
        if (humidity < -0.2f)
          return BIOME_DESERT;
        return BIOME_FOREST;
      } else if (temp < -0.3f) {
        return BIOME_TUNDRA;
      }
      if (humidity < -0.3f)
        return BIOME_PLAINS;
      return BIOME_FOREST;
    }
    return BIOME_OCEAN;
  }
  return ComputeBiome(x, z, y);
}

Biome WorldGenerator::GetBiomeAtHeight(int x, int z, int height) {
  // First get the base climate biome (pass height to account for lapse rate)
  Biome climateBiome = GetBiome(x, z, height);

  // Check if this location is underwater
  if (height < config.seaLevel) {
    // Determine if it's deep ocean or beach/shallow
    int depthBelowSea = config.seaLevel - height;

    if (depthBelowSea > 5) {
      return BIOME_OCEAN; // Deep water
    } else {
      return BIOME_BEACH; // Shallow water / beach transition
    }
  }

  // Above water - return the climate-based biome
  return climateBiome;
}

BlockType WorldGenerator::GetSurfaceBlock(int gx, int gy, int gz,
                                          bool checkCarving) {
  int height = GetHeight(gx, gz);
  float temp = GetTemperature(gx, gz, -1);
  float humid = GetHumidity(gx, gz);
  return GetSurfaceBlock(gx, gy, gz, height, temp, humid, checkCarving);
}

BlockType WorldGenerator::GetSurfaceBlock(int gx, int gy, int gz,
                                          int cachedHeight,
                                          float cachedBaseTemp,
                                          float cachedHumid,
                                          bool checkCarving) {
  int height = cachedHeight;
  if (gy > height)
    return AIR;

  // Re-calculate climate data logic locally to avoid noise calls
  // Apply altitude-based temperature decrease (lapse rate)
  float temp = cachedBaseTemp;
  if (gy != -1) {
    if (config.temperatureLapseRate > 0.0f && gy > config.seaLevel) {
      float altitudeAboveSeaLevel = (float)(gy - config.seaLevel);
      temp -= altitudeAboveSeaLevel * config.temperatureLapseRate;
    } else if (config.geothermalGradient > 0.0f && gy < config.seaLevel) {
      float depthBelowSeaLevel = (float)(config.seaLevel - gy);
      temp += depthBelowSeaLevel * config.geothermalGradient;
    }
  }

  float humidity = cachedHumid;

  BlockType surfaceBlock = GRASS;
  BlockType subsurfaceBlock = DIRT;

  // Determine surface blocks based on adjusted temperature and humidity
  if (temp < -0.4f) {
    // Very cold - snow and ice
    surfaceBlock = SNOW;
    subsurfaceBlock = DIRT;
  } else if (temp > 0.3f) {
    if (humidity < -0.2f) {
      surfaceBlock = SAND;
      subsurfaceBlock = SAND;
    } else {
      surfaceBlock = GRASS;
      subsurfaceBlock = DIRT;
    }
  } else if (temp < -0.3f) {
    if (humidity > 0.2f) {
      surfaceBlock = PODZOL;
      subsurfaceBlock = DIRT;
    } else {
      surfaceBlock = SNOW;
      subsurfaceBlock = DIRT;
    }
  } else {
    if (humidity > 0.4f) {
      surfaceBlock = MUD;
      subsurfaceBlock = DIRT;
    } else {
      surfaceBlock = GRASS;
      subsurfaceBlock = DIRT;
    }
  }

  // Beach Logic
  int beachOffX = (seed * 5432) % 65536;
  int beachOffZ = (seed * 1234) % 65536;
  float beachNoise = glm::perlin(glm::vec3(
      ((float)gx + beachOffX) * 0.05f, 0.0f, ((float)gz + beachOffZ) * 0.05f));
  int beachHeightLimit = config.seaLevel + (int)(beachNoise * 4.0f);
  BlockType beachBlock = (beachNoise > 0.4f) ? GRAVEL : SAND;

  BlockType type = AIR;
  if (gy == height) {
    if (gy < config.seaLevel)
      type = (beachNoise > 0.0f) ? GRAVEL : DIRT;
    else if (gy <= beachHeightLimit)
      type = beachBlock;
    else
      type = surfaceBlock;
  } else if (gy > height - config.surfaceDepth) {
    if (gy < config.seaLevel)
      type = DIRT;
    else
      type = subsurfaceBlock;
  } else {
    type = GetStrataBlock(gx, gy, gz);
  }

  // Cave/Ravine Carving Check
  if (checkCarving && type != AIR && type != WATER) {
    bool isUnderwater = (height <= config.seaLevel);
    bool preserveCrust = false;

    // Consistency check with GenerateChunk carving logic
    if (isUnderwater && gy > height - 3)
      preserveCrust = true;
    if (gy <= 0)
      preserveCrust = true;

    if (!preserveCrust) {
      if (caveGenerator->IsCaveAt(gx, gy, gz, height) ||
          caveGenerator->IsRavineAt(gx, gy, gz, height)) {
        if (gy <= config.lavaLevel)
          return LAVA;
        else
          return AIR;
      }
    }
  }

  return type;
}

bool WorldGenerator::IsCaveAt(int x, int y, int z) {
  if (!config.enableCaves && !config.enableRavines)
    return false;

  int surfaceHeight = GetHeight(x, z);
  if (y > surfaceHeight)
    return false;

  if (config.enableCaves && caveGenerator->IsCaveAt(x, y, z, surfaceHeight))
    return true;

  if (config.enableRavines && caveGenerator->IsRavineAt(x, y, z, surfaceHeight))
    return true;

  return false;
}

float WorldGenerator::GetCaveProbability(int x, int z) {
  if (!config.enableCaves)
    return 0.0f;

  // Use a simple 2D noise sample to show cave likelihood
  // This gives a more intuitive preview that responds to frequency changes
  int seedC = (seed * 7777) % 65536;
  float nx = (float)x + (float)seedC;
  float nz = (float)z + (float)seedC;

  float caveNoise = glm::perlin(glm::vec2(nx, nz) * config.caveFrequency);

  // Convert to 0-1 range and apply threshold
  // Higher values = more likely to have caves
  float normalized = (caveNoise + 1.0f) / 2.0f;

  // Return probability based on how far above threshold
  if (normalized > config.caveThreshold) {
    return (normalized - config.caveThreshold) / (1.0f - config.caveThreshold);
  }

  return 0.0f;
}

float WorldGenerator::GetLandformNoise(int x, int z) {
  // Very low frequency for large landform regions
  int seedL = (seed * 1111) % 65536;
  float nx = (float)x + (float)seedL;
  float nz = (float)z + (float)seedL;
  return glm::perlin(glm::vec2(nx, nz) * config.landformScale);
}

float WorldGenerator::GetClimateNoise(int x, int z) {
  // Low frequency for climate variation
  int seedC = (seed * 2222) % 65536;
  float nx = (float)x + (float)seedC;
  float nz = (float)z + (float)seedC;
  return glm::perlin(glm::vec2(nx, nz) * config.climateScale);
}

float WorldGenerator::GetGeologicNoise(int x, int z) {
  // Medium frequency for rock type variation
  int seedG = (seed * 3333) % 65536;
  float nx = (float)x + (float)seedG;
  float nz = (float)z + (float)seedG;
  return glm::perlin(glm::vec2(nx, nz) * config.geologicScale);
}

std::string WorldGenerator::GetLandformType(int x, int z) {
  float landformNoise = GetLandformNoise(x, z);

  // Map noise value to landform types (used for biome/decoration logic)
  if (landformNoise < -0.4f) {
    return "oceans";
  } else if (landformNoise < 0.0f) {
    return "valleys";
  } else if (landformNoise < 0.4f) {
    return "plains";
  } else if (landformNoise < 0.8f) {
    return "hills";
  } else {
    return "mountains";
  }
}

void WorldGenerator::InitializeLandforms() {
  // Oceans - deep basins
  LandformConfig oceans;
  oceans.name = "oceans";
  oceans.octaveAmplitudes = {0.60f,  0.20f,  0.10f,  0.05f,   0.025f,
                             0.012f, 0.006f, 0.003f, 0.0015f, 0.0008f};
  oceans.octaveThresholds = {0.0f, 0.0f, 0.0f, 0.0f, 0.0f,
                             0.0f, 0.0f, 0.0f, 0.0f, 0.0f};
  oceans.baseHeight = config.landformOverrides["oceans"].baseHeight;
  oceans.heightVariation = config.landformOverrides["oceans"].heightVariation;
  landforms["oceans"] = oceans;

  // Plains - smooth, gentle terrain
  LandformConfig plains;
  plains.name = "plains";
  plains.octaveAmplitudes = {0.55f,  0.28f,  0.14f,   0.07f,   0.035f,
                             0.018f, 0.009f, 0.0045f, 0.0022f, 0.0011f};
  plains.octaveThresholds = {0.0f, 0.0f, 0.0f, 0.0f, 0.0f,
                             0.0f, 0.0f, 0.0f, 0.0f, 0.0f};
  plains.baseHeight = config.landformOverrides["plains"].baseHeight;
  plains.heightVariation = config.landformOverrides["plains"].heightVariation;
  landforms["plains"] = plains;

  // Hills - moderate variation
  LandformConfig hills;
  hills.name = "hills";
  hills.octaveAmplitudes = {0.45f, 0.38f,  0.28f,  0.2f,   0.12f,
                            0.07f, 0.035f, 0.018f, 0.009f, 0.0045f};
  hills.octaveThresholds = {0.0f, 0.0f, 0.0f, 0.0f, 0.0f,
                            0.0f, 0.0f, 0.0f, 0.0f, 0.0f};
  hills.baseHeight = config.landformOverrides["hills"].baseHeight;
  hills.heightVariation = config.landformOverrides["hills"].heightVariation;
  landforms["hills"] = hills;

  // Mountains - dramatic, rugged terrain
  LandformConfig mountains;
  mountains.name = "mountains";
  mountains.octaveAmplitudes = {0.38f, 0.45f, 0.5f,  0.42f,  0.28f,
                                0.2f,  0.14f, 0.07f, 0.035f, 0.018f};
  mountains.octaveThresholds = {0.0f, 0.0f, 0.0f, 0.0f, 0.0f,
                                0.0f, 0.0f, 0.0f, 0.0f, 0.0f};
  mountains.baseHeight = config.landformOverrides["mountains"].baseHeight;
  mountains.heightVariation =
      config.landformOverrides["mountains"].heightVariation;
  landforms["mountains"] = mountains;

  // Valleys - low, flat areas
  LandformConfig valleys;
  valleys.name = "valleys";
  valleys.octaveAmplitudes = {0.65f,  0.22f,  0.11f,   0.055f,  0.028f,
                              0.014f, 0.007f, 0.0035f, 0.0017f, 0.0008f};
  valleys.octaveThresholds = {0.0f, 0.0f, 0.0f, 0.0f, 0.0f,
                              0.0f, 0.0f, 0.0f, 0.0f, 0.0f};
  valleys.baseHeight = config.landformOverrides["valleys"].baseHeight;
  valleys.heightVariation = config.landformOverrides["valleys"].heightVariation;
  landforms["valleys"] = valleys;
}

BlockType WorldGenerator::GetStrataBlock(int x, int y, int z) {
  // Stylized stone layers
  // Primary: Stone, with horizontal layers of variants for visual interest

  int seedS = (seed * 777) % 65536;
  float nx = (float)x + (float)seedS;
  float ny = (float)y;
  float nz = (float)z + (float)seedS;

  // 2D noise for layer undulation (wavy boundaries)
  float layerWave = glm::perlin(glm::vec2(nx * 0.02f, nz * 0.02f));
  int adjustedY = y + (int)(layerWave * 5.0f);

  // Secondary noise for layer type variation
  float typeNoise = glm::perlin(glm::vec2(nx * 0.01f, nz * 0.01f));

  // Horizontal layers with some variation
  // Deep layers
  if (adjustedY < 12) {
    if (typeNoise > 0.3f)
      return GRANITE;
    else if (typeNoise < -0.3f)
      return BASALT;
    else
      return DIORITE;
  }
  // Stone layer
  else if (adjustedY < 20) {
    return STONE;
  }
  // Mid variant layer
  else if (adjustedY < 25) {
    if (typeNoise > 0.2f)
      return ANDESITE;
    else
      return TUFF;
  }
  // Stone layer
  else if (adjustedY < 35) {
    return STONE;
  }
  // Upper variant layer
  else if (adjustedY < 40) {
    if (typeNoise > 0.0f)
      return SANDSTONE;
    else
      return DIORITE;
  }
  // Stone layer (most common near surface)
  else {
    return STONE;
  }
}

#include "ChunkColumn.h"

void WorldGenerator::GenerateColumn(ChunkColumn &column, int cx, int cz) {
  PROFILE_SCOPE_CONDITIONAL("GenColumn", m_ProfilingEnabled);
  for (int x = 0; x < CHUNK_SIZE; ++x) {
    for (int z = 0; z < CHUNK_SIZE; ++z) {
      int gx = cx * CHUNK_SIZE + x;
      int gz = cz * CHUNK_SIZE + z;
      column.heightMap[x][z] = GetHeight(gx, gz);
      // Cache climate data
      column.temperatureMap[x][z] = GetTemperature(gx, gz, -1);
      column.humidityMap[x][z] = GetHumidity(gx, gz);

      column.biomeMap[x][z] = GetBiomeAtHeight(gx, gz, column.heightMap[x][z]);
    }
  }
}

void WorldGenerator::GenerateChunk(Chunk &chunk, const ChunkColumn &column) {
  PROFILE_SCOPE_CONDITIONAL("GenChunk", m_ProfilingEnabled);
  glm::ivec3 pos = chunk.chunkPosition;

  // Pass 1: Terrain
  {
    PROFILE_SCOPE_CONDITIONAL("GenChunk_Terrain", m_ProfilingEnabled);
    for (int x = 0; x < CHUNK_SIZE; ++x) {
      for (int z = 0; z < CHUNK_SIZE; ++z) {
        // Global Coordinates
        int gx = pos.x * CHUNK_SIZE + x;
        int gz = pos.z * CHUNK_SIZE + z;

        int height = column.heightMap[x][z];
        float baseTemp = column.temperatureMap[x][z];
        float humid = column.humidityMap[x][z];

        for (int y = 0; y < CHUNK_SIZE; ++y) {
          int gy = pos.y * CHUNK_SIZE + y;
          BlockType type = GetSurfaceBlock(gx, gy, gz, height, baseTemp, humid);
          chunk.setBlock(x, y, z, type);
        }
      }
    }
  }

  // Pass 2: Water & Ice
  {
    PROFILE_SCOPE_CONDITIONAL("GenChunk_Water", m_ProfilingEnabled);
    for (int x = 0; x < CHUNK_SIZE; ++x) {
      for (int z = 0; z < CHUNK_SIZE; ++z) {
        int gx = pos.x * CHUNK_SIZE + x;
        int gz = pos.z * CHUNK_SIZE + z;

        for (int y = 0; y < CHUNK_SIZE; ++y) {
          int gy = pos.y * CHUNK_SIZE + y;
          if (chunk.getBlock(x, y, z).getType() == AIR &&
              gy <= config.seaLevel) {
            float temp = column.temperatureMap[x][z];
            // Apply lapse rate for Ice check at sea level
            if (gy > config.seaLevel) {
              float alt = (float)(gy - config.seaLevel);
              temp -= alt * config.temperatureLapseRate;
            }

            if (temp < -0.3f && gy == config.seaLevel)
              chunk.setBlock(x, y, z, ICE);
            else
              chunk.setBlock(x, y, z, WATER);
          }
        }
      }
    }
  }

  // Pass 3: Caves & Ravines
  {
    PROFILE_SCOPE_CONDITIONAL("GenChunk_Caves", m_ProfilingEnabled);
    if (config.enableCaves || config.enableRavines) {
      for (int x = 0; x < CHUNK_SIZE; ++x) {
        for (int z = 0; z < CHUNK_SIZE; ++z) {
          int gx = pos.x * CHUNK_SIZE + x;
          int gz = pos.z * CHUNK_SIZE + z;
          int height = column.heightMap[x][z];
          bool isUnderwater = (height <= config.seaLevel);

          for (int y = 0; y < CHUNK_SIZE; ++y) {
            int gy = pos.y * CHUNK_SIZE + y;

            // Optimization: Skip Air (unless we want caves in air? No)
            // But caves carve stone.
            BlockType currentType =
                (BlockType)chunk.getBlock(x, y, z).getType();
            if (currentType == AIR || currentType == WATER ||
                currentType == LAVA)
              continue;

            bool preserveCrust = false;
            if (isUnderwater && gy > height - 3)
              preserveCrust = true;
            if (gy <= 0)
              preserveCrust = true;

            if (!preserveCrust) {
              bool shouldCarve = false;
              if (config.enableCaves &&
                  caveGenerator->IsCaveAt(gx, gy, gz, height))
                shouldCarve = true;
              else if (config.enableRavines &&
                       caveGenerator->IsRavineAt(gx, gy, gz, height))
                shouldCarve = true;

              if (shouldCarve) {
                if (gy <= config.lavaLevel)
                  chunk.setBlock(x, y, z, LAVA);
                else
                  chunk.setBlock(x, y, z, AIR);
              }
            }
          }
        }
      }
    }
  }

  // Pass 4: Final Touches (Bedrock & Post-fix)
  {
    PROFILE_SCOPE_CONDITIONAL("GenChunk_Finishing", m_ProfilingEnabled);
    for (int x = 0; x < CHUNK_SIZE; ++x) {
      for (int z = 0; z < CHUNK_SIZE; ++z) {
        int gx = pos.x * CHUNK_SIZE + x;
        int gz = pos.z * CHUNK_SIZE + z;
        int height = column.heightMap[x][z];
        float baseTemp = column.temperatureMap[x][z];
        float humid = column.humidityMap[x][z];

        for (int y = 0; y < CHUNK_SIZE; ++y) {
          int gy = pos.y * CHUNK_SIZE + y;
          BlockType current = (BlockType)chunk.getBlock(x, y, z).getType();

          // Bedrock Flattening
          if (gy <= 4 && current != AIR && current != LAVA) {
            if (gy > 0) {
              int chance = 100 - (gy * 20);
              if ((rand() % 100) < chance) {
                // Re-call Surface Block gen for bedrock replacement?
                // Original logic called GetSurfaceBlock again.
                // We can use cached args.
                BlockType type =
                    GetSurfaceBlock(gx, gy, gz, height, baseTemp, humid);
                chunk.setBlock(x, y, z, type);
              }
            }
          }

          // Grass->Dirt under water fix
          if (current == WATER && gy <= config.seaLevel) {
            if (y > 0) {
              if (chunk.getBlock(x, y - 1, z).getType() == GRASS) {
                chunk.setBlock(x, y - 1, z, DIRT);
              }
            }
          }
        }
      }
    }
  }

  // Apply Decorators
  {
    PROFILE_SCOPE_CONDITIONAL("Decorators", m_ProfilingEnabled);
    for (auto d : decorators) {
      d->Decorate(chunk, *this, column);
    }
  }
}
