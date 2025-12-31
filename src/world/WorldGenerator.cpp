#include "WorldGenerator.h"
#include "../debug/Logger.h"
#include "../debug/Profiler.h"
#include "Block.h"
#include "Chunk.h"
#include "ChunkColumn.h"
#include <glm/glm.hpp>
#include <glm/gtc/noise.hpp>

#include "FloraDecorator.h"
#include "OreDecorator.h"
#include "TreeDecorator.h"
#include <FastNoise/FastNoise.h>
#include <mutex>

WorldGenerator::WorldGenerator(const WorldGenConfig &config)
    : config(config), m_Seed(config.seed) {
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
  caveGenerator->generator = this; // Give cave generator access to FastNoise
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

  int seedX = (m_Seed * 1337) % 65536;
  int seedZ = (m_Seed * 9999) % 65536;
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
    float octaveValue = FastNoise2D(nx * frequency, nz * frequency, i);

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

  int seedR = (m_Seed * 1234) % 65536;
  float rx = (float)x + (float)seedR;
  float rz = (float)z + (float)seedR;

  float riverNoise =
      FastNoise2D(rx * config.riverScale, rz * config.riverScale, 600);
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

  int seedOffX = (m_Seed * 1337) % 65536;
  int seedOffZ = (m_Seed * 9999) % 65536;
  float nx = (float)x + (float)seedOffX;
  float nz = (float)z + (float)seedOffZ;

  const int numOctaves = 10;
  float noiseHeight = 0.0f;
  float frequency = config.terrainScale;

  for (int i = 0; i < numOctaves; ++i) {
    float octaveValue = m_PerlinNoise2D->GenSingle2D(
        nx * frequency, nz * frequency, m_Seed + i);

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

int WorldGenerator::GetHeight(int gx, int gz) {
  if (!m_HeightFractal)
    InitializeFastNoise();

  float lNoise = GetLandformNoise(gx, gz);
  float nx = (float)gx * config.terrainScale;
  float nz = (float)gz * config.terrainScale;
  float hNoise = m_HeightFractal->GenSingle2D(nx, nz, m_Seed);

  int baseFinalHeight = CalculateHeightFromNoise(hNoise, lNoise);

  // River Carving
  if (config.enableRivers) {
    float carveFactor = GetRiverCarveFactor(gx, gz);
    if (carveFactor > 0.0f) {
      // Ensure it reaches sea level + some channel depth
      float heightToSea = (float)(baseFinalHeight - config.seaLevel);
      float dynamicDepth = std::max(config.riverDepth, heightToSea + 2.0f);
      baseFinalHeight -= (int)(dynamicDepth * carveFactor);
    }
  }

  return baseFinalHeight;
}

int WorldGenerator::CalculateHeightFromNoise(float hNoise, float lNoise) const {
  std::string primary_name, secondary_name;
  float blendFactor;
  if (lNoise < -0.4f) {
    primary_name = "oceans";
    secondary_name = "valleys";
    blendFactor = (lNoise + 0.6f) / 0.2f;
  } else if (lNoise < 0.0f) {
    primary_name = "valleys";
    secondary_name = "plains";
    blendFactor = (lNoise + 0.4f) / 0.4f;
  } else if (lNoise < 0.4f) {
    primary_name = "plains";
    secondary_name = "hills";
    blendFactor = (lNoise - 0.0f) / 0.4f;
  } else {
    primary_name = "hills";
    secondary_name = "mountains";
    blendFactor = (lNoise - 0.4f) / 0.4f;
  }
  blendFactor = std::max(0.0f, std::min(1.0f, blendFactor));

  const auto &pConfig = config.landformOverrides.at(primary_name);
  const auto &sConfig = config.landformOverrides.at(secondary_name);

  // Apply terrain smoothing
  float smoothedH = (hNoise + 1.0f) * 0.5f;
  smoothedH = pow(smoothedH, 1.2f);

  float h1 = pConfig.baseHeight + smoothedH * pConfig.heightVariation;
  float h2 = sConfig.baseHeight + smoothedH * sConfig.heightVariation;
  return (int)(h1 * (1.0f - blendFactor) + h2 * blendFactor);
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
  if (!m_TemperatureNoise)
    InitializeFastNoise();

  // Very low frequency noise for large biomes
  int seedT = (m_Seed * 555) % 65536;
  float nx = (float)x + (float)seedT;
  float nz = (float)z + (float)seedT;
  float temp = m_TemperatureNoise->GenSingle2D(
      nx * config.tempScale, nz * config.tempScale, m_Seed + 100);

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
  if (!m_HumidityNoise)
    InitializeFastNoise();

  int seedH = (m_Seed * 888) % 65536;
  float nx = (float)x + (float)seedH;
  float nz = (float)z + (float)seedH;
  return m_HumidityNoise->GenSingle2D(nx * config.humidityScale,
                                      nz * config.humidityScale, m_Seed + 200);
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
  return ComputeBiome(x, z, y, -1.0f, -1.0f);
}

Biome WorldGenerator::ComputeBiome(int x, int z, int y, float preTemp,
                                   float preHumid) {
  float temperature = preTemp;
  float humidity = preHumid;

  if (temperature < -0.9f)
    temperature = GetTemperature(x, z, y);
  if (humidity < -0.9f)
    humidity = GetHumidity(x, z);

  // Add high-frequency variation noise to break up smooth blobs
  if (config.biomeVariation > 0.0f) {
    if (!m_PerlinNoise2D)
      InitializeFastNoise();

    int seedV = (m_Seed * 5555) % 65536;
    float vx = (float)x + (float)seedV;
    float vz = (float)z + (float)seedV;

    // Use much higher frequency noise for variation (0.05 instead of 0.02)
    float varNoise1 =
        m_PerlinNoise2D->GenSingle2D(vx * 0.05f, vz * 0.05f, m_Seed + 1);
    float varNoise2 = m_PerlinNoise2D->GenSingle2D(
        vx * 1.3f * 0.08f, vz * 1.3f * 0.08f, m_Seed + 2);

    // Combine two noise layers for more complex patterns
    float combinedNoise = (varNoise1 + varNoise2 * 0.5f) / 1.5f;

    // Apply much stronger variation - multiply by 2 to make it very noticeable
    temperature += combinedNoise * config.biomeVariation * 2.0f;
    humidity += combinedNoise * config.biomeVariation * 1.6f;
  }

  // Normalize logic slightly if needed, but perlin is approx -1 to 1

  if (temperature > 0.3f) {
    // Hot
    if (humidity < -0.2f)
      return BIOME_DESERT; // Hot and Dry
    return BIOME_FOREST;   // Hot and Wet (Jungle-ish)
  } else if (temperature < -0.3f) {
    // Cold - significantly cold becomes Tundra
    return BIOME_TUNDRA;
  }

  // Moderate Temp
  if (humidity < -0.3f)
    return BIOME_PLAINS;
  return BIOME_FOREST;
}

Biome WorldGenerator::GetBiomeAtHeight(int x, int z, int height, float temp,
                                       float humid) {
  // First get the base climate biome (pass height to account for lapse rate)
  Biome climateBiome = ComputeBiome(x, z, height, temp, humid);

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
  float beach = GetBeachNoise(gx, gz);

  return GetSurfaceBlock(gx, gy, gz, height, temp, humid, beach, nullptr,
                         checkCarving);
}

BlockType WorldGenerator::GetSurfaceBlock(int gx, int gy, int gz, int height,
                                          float temp, float humid,
                                          float beachNoise,
                                          const ChunkColumn *column,
                                          bool checkCarving) {
  // Use pre-computed values for performance!
  int beachHeightLimit = config.seaLevel + 3;
  if (gy > height)
    return AIR;

  // Re-calculate climate data logic locally to avoid noise calls
  // Apply altitude-based temperature decrease (lapse rate)
  float adjustedTemp = temp;
  if (gy != -1) {
    if (config.temperatureLapseRate > 0.0f && gy > config.seaLevel) {
      float altitudeAboveSeaLevel = (float)(gy - config.seaLevel);
      adjustedTemp -= altitudeAboveSeaLevel * config.temperatureLapseRate;
    } else if (config.geothermalGradient > 0.0f && gy < config.seaLevel) {
      float depthBelowSeaLevel = (float)(config.seaLevel - gy);
      adjustedTemp += depthBelowSeaLevel * config.geothermalGradient;
    }
  }

  float adjustedHumidity = humid;

  BlockType surfaceBlock = GRASS;
  BlockType subsurfaceBlock = DIRT;

  // Determine surface blocks based on adjusted temperature and humidity
  if (adjustedTemp < -0.4f) {
    // Very cold - snow and ice
    surfaceBlock = SNOW;
    subsurfaceBlock = DIRT;
  } else if (adjustedTemp > 0.3f) {
    if (adjustedHumidity < -0.2f) {
      surfaceBlock = SAND;
      subsurfaceBlock = SAND;
    } else {
      surfaceBlock = GRASS;
      subsurfaceBlock = DIRT;
    }
  } else if (adjustedTemp < -0.3f) {
    if (adjustedHumidity > 0.2f) {
      surfaceBlock = PODZOL;
      subsurfaceBlock = DIRT;
    } else {
      surfaceBlock = SNOW;
      subsurfaceBlock = DIRT;
    }
  } else {
    if (adjustedHumidity > 0.4f) {
      surfaceBlock = MUD;
      subsurfaceBlock = DIRT;
    } else {
      surfaceBlock = GRASS;
      subsurfaceBlock = DIRT;
    }
  }

  // Beach Logic (use cached value)
  int dynamicBeachHeight = config.seaLevel + (int)(beachNoise * 4.0f);
  BlockType beachBlock = (beachNoise > 0.4f) ? GRAVEL : SAND;

  BlockType type = AIR;
  if (gy == height) {
    if (gy < config.seaLevel)
      type = (beachNoise > 0.0f) ? GRAVEL : DIRT;
    else if (gy <= dynamicBeachHeight || gy <= beachHeightLimit)
      type = beachBlock;
    else
      type = surfaceBlock;
  } else if (gy > height - config.surfaceDepth) {
    if (gy < config.seaLevel)
      type = DIRT;
    else
      type = subsurfaceBlock;
  } else {
    // Determine strata using pre-computed values if available
    float layerWave = 0.0f;
    float typeNoise = 0.0f;

    if (column) {
      layerWave = column->strataWaveMap[gx % CHUNK_SIZE][gz % CHUNK_SIZE];
      typeNoise = column->strataTypeMap[gx % CHUNK_SIZE][gz % CHUNK_SIZE];
    } else {
      int seedS = (m_Seed * 777) % 65536;
      layerWave = m_PerlinNoise2D->GenSingle2D((float)gx + seedS,
                                               (float)gz + seedS, m_Seed + 300);
      typeNoise = m_PerlinNoise2D->GenSingle2D((float)gx + seedS,
                                               (float)gz + seedS, m_Seed + 400);
    }

    int adjustedY = gy + (int)(layerWave * 5.0f);

    if (adjustedY < 12) {
      if (typeNoise > 0.3f)
        type = GRANITE;
      else if (typeNoise < -0.3f)
        type = BASALT;
      else
        type = DIORITE;
    } else if (adjustedY < 20) {
      type = STONE;
    } else if (adjustedY < 25) {
      if (typeNoise > 0.2f)
        type = ANDESITE;
      else
        type = TUFF;
    } else {
      type = STONE;
    }
  }

  // Cave/Ravine Carving Check
  if (checkCarving && type != AIR && type != WATER) {
    bool isUnderwater = (height <= config.seaLevel);
    bool preserveCrust = false;

    // Consistency check with GenerateChunk carving logic
    if (gy <= 0) {
      preserveCrust = true;
    } else if (isUnderwater) {
      if (gy > height - 3)
        preserveCrust = true;
    } else {
      // Smart Crust: Protect top 2 blocks unless in an entrance zone
      if (gy >= height - 2) {
        int sX = (m_Seed * 7777) % 65536;
        int sZ = (m_Seed * 9999) % 65536;
        float entrance = FastNoise2D((float)(gx + sX) * 0.012f,
                                     (float)(gz + sZ) * 0.012f, 5000);
        if (entrance < config.caveEntranceNoise) {
          preserveCrust = true;
        }
      }
    }

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
  int seedC = (m_Seed * 7777) % 65536;
  float nx = (float)x + (float)seedC;
  float nz = (float)z + (float)seedC;

  float caveNoise =
      FastNoise2D(nx * config.caveFrequency, nz * config.caveFrequency, 777);

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
  int seedL = (m_Seed * 1111) % 65536;
  float nx = (float)x + (float)seedL;
  float nz = (float)z + (float)seedL;
  return m_LandformNoise->GenSingle2D(nx * config.landformScale,
                                      nz * config.landformScale, m_Seed + 500);
}

float WorldGenerator::GetClimateNoise(int x, int z) {
  // Low frequency for climate variation
  int seedC = (m_Seed * 2222) % 65536;
  float nx = (float)x + (float)seedC;
  float nz = (float)z + (float)seedC;
  float noise =
      FastNoise2D(nx * config.climateScale, nz * config.climateScale, 222);
  return noise;
}

float WorldGenerator::GetGeologicNoise(int x, int z) {
  // Medium frequency for rock type variation
  int seedG = (m_Seed * 3333) % 65536;
  float nx = (float)x + (float)seedG;
  float nz = (float)z + (float)seedG;
  return m_PerlinNoise2D->GenSingle2D(nx * config.geologicScale,
                                      nz * config.geologicScale, m_Seed + 333);
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

  int seedS = (m_Seed * 777) % 65536;
  float nx = (float)x + (float)seedS;
  float ny = (float)y;
  float nz = (float)z + (float)seedS;

  // 2D noise for layer undulation (wavy boundaries)
  if (!m_PerlinNoise2D)
    InitializeFastNoise();

  float layerWave =
      m_PerlinNoise2D->GenSingle2D(nx * 0.02f, nz * 0.02f, m_Seed + 300);
  int adjustedY = y + (int)(layerWave * 5.0f);

  // Secondary noise for layer type variation
  float typeNoise =
      m_PerlinNoise2D->GenSingle2D(nx * 0.01f, nz * 0.01f, m_Seed + 400);

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
  if (!m_HeightFractal)
    InitializeFastNoise();
  PROFILE_SCOPE_CONDITIONAL("GenColumn", m_ProfilingEnabled);

  int startX = cx * CHUNK_SIZE;
  int startZ = cz * CHUNK_SIZE;

  // 1. Batch generate Height Map (use vector for alignment)
  std::vector<float> heightNoise(CHUNK_SIZE * CHUNK_SIZE);
  m_HeightFractal->GenUniformGrid2D(heightNoise.data(), startX, startZ,
                                    CHUNK_SIZE, CHUNK_SIZE, config.terrainScale,
                                    m_Seed);

  // 2. Batch generate Temperature Map
  std::vector<float> tempNoise(CHUNK_SIZE * CHUNK_SIZE);
  int seedT = (m_Seed * 555) % 65536;
  m_TemperatureNoise->GenUniformGrid2D(
      tempNoise.data(), (float)startX + seedT, (float)startZ + seedT,
      CHUNK_SIZE, CHUNK_SIZE, config.tempScale, m_Seed + 100);

  // 3. Batch generate Humidity Map
  std::vector<float> humidNoise(CHUNK_SIZE * CHUNK_SIZE);
  int seedH = (m_Seed * 888) % 65536;
  m_HumidityNoise->GenUniformGrid2D(
      humidNoise.data(), (float)startX + seedH, (float)startZ + seedH,
      CHUNK_SIZE, CHUNK_SIZE, config.humidityScale, m_Seed + 200);

  // 4. Batch generate Beach Noise Map
  std::vector<float> beachNoise(CHUNK_SIZE * CHUNK_SIZE);
  int seedBX = (m_Seed * 5432) % 65536;
  int seedBZ = (m_Seed * 1234) % 65536;
  m_BeachNoise->GenUniformGrid2D(beachNoise.data(), (float)startX + seedBX,
                                 (float)startZ + seedBZ, CHUNK_SIZE, CHUNK_SIZE,
                                 0.05f, m_Seed);

  // 5. Batch generate Landform Noise Map
  std::vector<float> landformNoise(CHUNK_SIZE * CHUNK_SIZE);
  int seedL = (m_Seed * 1111) % 65536;
  m_LandformNoise->GenUniformGrid2D(
      landformNoise.data(), (float)startX + seedL, (float)startZ + seedL,
      CHUNK_SIZE, CHUNK_SIZE, config.landformScale, m_Seed + 500);

  // 6. Batch generate Strata Noise Maps
  std::vector<float> strataWave(CHUNK_SIZE * CHUNK_SIZE);
  int seedS = (m_Seed * 777) % 65536;
  m_PerlinNoise2D->GenUniformGrid2D(strataWave.data(), (float)startX + seedS,
                                    (float)startZ + seedS, CHUNK_SIZE,
                                    CHUNK_SIZE, 0.02f, m_Seed + 300);

  std::vector<float> strataType(CHUNK_SIZE * CHUNK_SIZE);
  m_PerlinNoise2D->GenUniformGrid2D(strataType.data(), (float)startX + seedS,
                                    (float)startZ + seedS, CHUNK_SIZE,
                                    CHUNK_SIZE, 0.01f, m_Seed + 400);

  // Process and store in column
  for (int z = 0; z < CHUNK_SIZE; ++z) {
    for (int x = 0; x < CHUNK_SIZE; ++x) {
      int idx = x + z * CHUNK_SIZE;

      // Determine height with landform variety
      float lNoise = landformNoise[idx];
      float hNoise = heightNoise[idx];
      int height = CalculateHeightFromNoise(hNoise, lNoise);

      // Apply River Carving (individual check for now, but could be batched)
      if (config.enableRivers) {
        float carve = GetRiverCarveFactor(startX + x, startZ + z);
        if (carve > 0.0f) {
          float hToSea = (float)(height - config.seaLevel);
          height -= (int)(std::max(config.riverDepth, hToSea + 2.0f) * carve);
        }
      }

      column.heightMap[x][z] = height;
      column.temperatureMap[x][z] = tempNoise[idx];
      column.humidityMap[x][z] = humidNoise[idx];
      column.beachNoiseMap[x][z] = beachNoise[idx];
      column.strataWaveMap[x][z] = strataWave[idx];
      column.strataTypeMap[x][z] = strataType[idx];
      column.biomeMap[x][z] = GetBiomeAtHeight(startX + x, startZ + z, height,
                                               tempNoise[idx], humidNoise[idx]);
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
        float beachNoise = column.beachNoiseMap[x][z];

        for (int y = 0; y < CHUNK_SIZE; ++y) {
          int gy = pos.y * CHUNK_SIZE + y;
          BlockType type = GetSurfaceBlock(gx, gy, gz, height, baseTemp, humid,
                                           beachNoise, &column);
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
    // LOG_WORLD_INFO("GenChunk Caves Start: {}, {}, {}", pos.x, pos.y, pos.z);
    PROFILE_SCOPE_CONDITIONAL("GenChunk_Caves", m_ProfilingEnabled);

    // Generate all cave noise grids once using SIMD batch (massive speedup!)
    // Allocated on HEAP to prevent stack overflow
    auto caveNoise = std::make_unique<CaveNoiseData>();
    GenerateCaveNoiseData(*caveNoise, pos.x, pos.z, pos.y);

    if (config.enableCaves || config.enableRavines) {
      for (int x = 0; x < CHUNK_SIZE; ++x) {
        for (int z = 0; z < CHUNK_SIZE; ++z) {
          int gx = pos.x * CHUNK_SIZE + x;
          int gz = pos.z * CHUNK_SIZE + z;
          int height = column.heightMap[x][z];
          bool isUnderwater = (height < config.seaLevel);

          // Pre-calculate entrance zone for this column to avoid inner-loop
          // lookups
          int idx2D = caveNoise->Index2D(x + 2, z + 2);
          float entrance = caveNoise->entranceNoise[idx2D];
          bool inEntranceZone = (entrance >= config.caveEntranceNoise);

          for (int y = 0; y < CHUNK_SIZE; ++y) {
            int gy = pos.y * CHUNK_SIZE + y;
            if (gy > height || gy > CHUNK_SIZE * 8)
              break;

            // Only carve natural blocks, not air/water/lava.
            BlockType currentType =
                (BlockType)chunk.getBlock(x, y, z).getType();
            if (currentType == AIR || currentType == WATER ||
                currentType == LAVA)
              continue;

            bool preserveCrust = false;
            if (gy <= 0) {
              preserveCrust = true;
            } else if (isUnderwater) {
              if (gy > height - 3)
                preserveCrust = true;
            } else {
              // Smart Crust: Protect top 2 blocks unless in an entrance zone
              if (gy >= height - 2 && !inEntranceZone)
                preserveCrust = true;
            }

            if (!preserveCrust) {
              bool shouldCarve = false;
              if (config.enableCaves &&
                  caveGenerator->IsCaveAt(x, y, z, height, *caveNoise))
                shouldCarve = true;
              else if (config.enableRavines &&
                       caveGenerator->IsRavineAt(x, y, z, gx, gy, gz, height,
                                                 *caveNoise))
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
        float beachNoise = column.beachNoiseMap[x][z];

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
                BlockType type = GetSurfaceBlock(gx, gy, gz, height, baseTemp,
                                                 humid, beachNoise, &column);
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
      // Profile each decorator individually
      const char *decoratorName = "Decorator_Unknown";
      if (dynamic_cast<OreDecorator *>(d))
        decoratorName = "Decorator_Ores";
      else if (dynamic_cast<TreeDecorator *>(d))
        decoratorName = "Decorator_Trees";
      else if (dynamic_cast<FloraDecorator *>(d))
        decoratorName = "Decorator_Flora";

      PROFILE_SCOPE_CONDITIONAL(decoratorName, m_ProfilingEnabled);
      d->Decorate(chunk, *this, column);
    }
  }
}

// FastNoise2 Wrapper Implementation
void WorldGenerator::InitializeFastNoise() {
  std::lock_guard<std::mutex> lock(m_InitMutex);
  if (m_Initialized)
    return;

  // 1. Basic 2D/3D Perlin (Upcast to SmartNode<>)
  m_PerlinNoise2D = FastNoise::New<FastNoise::Perlin>();
  m_PerlinNoise3D = FastNoise::New<FastNoise::Perlin>();

  // 2. Specialized Nodes
  m_TemperatureNoise = FastNoise::New<FastNoise::Perlin>();
  m_HumidityNoise = FastNoise::New<FastNoise::Perlin>();
  m_BeachNoise = FastNoise::New<FastNoise::Perlin>();
  m_LandformNoise = FastNoise::New<FastNoise::Perlin>();

  // 3. Fractal FBM for Terrain (requires specialized configuration)
  auto fractal = FastNoise::New<FastNoise::FractalFBm>();
  auto terrainSource = FastNoise::New<FastNoise::Perlin>();

  fractal->SetSource(terrainSource);
  fractal->SetOctaveCount(10);
  fractal->SetGain(0.5f);
  fractal->SetLacunarity(2.0f);

  m_HeightFractal = fractal;

  m_Initialized = true;
  LOG_INFO("FastNoise2 SIMD nodes initialized");
}

float WorldGenerator::FastNoise2D(float x, float y, int seedOffset) {
  if (!m_PerlinNoise2D) {
    InitializeFastNoise();
  }

  // FastNoise2 returns float directly
  return m_PerlinNoise2D->GenSingle2D(x, y, m_Seed + seedOffset);
}

float WorldGenerator::FastNoise3D(float x, float y, float z, int seedOffset) {
  if (!m_PerlinNoise3D) {
    InitializeFastNoise();
  }

  // FastNoise2 returns float directly
  return m_PerlinNoise3D->GenSingle3D(x, y, z, m_Seed + seedOffset);
}

// Batch grid generation - SIMD optimized
void WorldGenerator::FastNoiseGrid2D(float *output, int startX, int startZ,
                                     int width, int height, float frequency,
                                     int seedOffset) {
  if (!m_PerlinNoise2D) {
    InitializeFastNoise();
  }

  // Generate entire grid at once using SIMD
  m_PerlinNoise2D->GenUniformGrid2D(output, startX, startZ, width, height,
                                    frequency, m_Seed + seedOffset);
}

void WorldGenerator::GenerateTemperatureGrid(float *output, int startX,
                                             int startZ, int width,
                                             int height) const {
  if (!m_TemperatureNoise)
    const_cast<WorldGenerator *>(this)->InitializeFastNoise();

  int seedT = (m_Seed * 555) % 65536;
  m_TemperatureNoise->GenUniformGrid2D(output, (float)startX + seedT,
                                       (float)startZ + seedT, width, height,
                                       config.tempScale, m_Seed + 100);
}

void WorldGenerator::GenerateHumidityGrid(float *output, int startX, int startZ,
                                          int width, int height) const {
  if (!m_HumidityNoise)
    const_cast<WorldGenerator *>(this)->InitializeFastNoise();

  int seedH = (m_Seed * 888) % 65536;
  m_HumidityNoise->GenUniformGrid2D(output, (float)startX + seedH,
                                    (float)startZ + seedH, width, height,
                                    config.humidityScale, m_Seed + 200);
}

void WorldGenerator::GenerateBeachGrid(float *output, int startX, int startZ,
                                       int width, int height) const {
  if (!m_BeachNoise)
    const_cast<WorldGenerator *>(this)->InitializeFastNoise();

  int seedBX = (m_Seed * 5432) % 65536;
  int seedBZ = (m_Seed * 1234) % 65536;
  m_BeachNoise->GenUniformGrid2D(output, (float)startX + seedBX,
                                 (float)startZ + seedBZ, width, height, 0.05f,
                                 m_Seed);
}

void WorldGenerator::GenerateHeightGrid(float *output, int startX, int startZ,
                                        int width, int height) const {
  if (!m_HeightFractal)
    const_cast<WorldGenerator *>(this)->InitializeFastNoise();

  m_HeightFractal->GenUniformGrid2D(output, startX, startZ, width, height,
                                    config.terrainScale, m_Seed);
}

void WorldGenerator::GenerateLandformGrid(float *output, int startX, int startZ,
                                          int width, int height) const {
  if (!m_LandformNoise)
    const_cast<WorldGenerator *>(this)->InitializeFastNoise();

  int seedL = (m_Seed * 1111) % 65536;
  m_LandformNoise->GenUniformGrid2D(output, (float)startX + seedL,
                                    (float)startZ + seedL, width, height,
                                    config.landformScale, m_Seed + 500);
}

// Batch 3D grid generation - SIMD optimized for caves
void WorldGenerator::FastNoiseGrid3D(float *output, int startX, int startY,
                                     int startZ, int width, int height,
                                     int depth, float frequency,
                                     int seedOffset) {
  if (!m_PerlinNoise3D) {
    InitializeFastNoise();
  }

  // Generate entire 3D grid at once using SIMD
  m_PerlinNoise3D->GenUniformGrid3D(output, startX, startY, startZ, width,
                                    height, depth, frequency,
                                    m_Seed + seedOffset);
}

// Generate all cave noise grids for a chunk in one batch (SIMD optimized)
void WorldGenerator::GenerateCaveNoiseData(CaveNoiseData &data, int chunkX,
                                           int chunkZ, int chunkY) {
  const int SIZE = CaveNoiseData::SIZE; // 36x36x36
  const int chunkWorldX = chunkX * 32;  // CHUNK_SIZE = 32
  const int chunkWorldY = chunkY * 32;
  const int chunkWorldZ = chunkZ * 32;

  // Calculate seeds
  if (!m_PerlinNoise3D)
    InitializeFastNoise();

  int seedX = (m_Seed * 7777) % 65536;
  int seedY = (m_Seed * 8888) % 65536;
  int seedZ = (m_Seed * 9999) % 65536;

  // Calculate scales
  float cheeseScale = 0.01f * (config.caveFrequency / 0.015f);
  float spagModScale = 0.01f * (config.caveFrequency / 0.015f);
  float spagNoiseScale = 0.03f * (config.caveFrequency / 0.015f);

  // Generate all 4 3D noise grids using SIMD batch
  // Start at -2 for padding, size 36 = 32 + 4
  int startX = chunkWorldX - 2 + seedX;
  int startY = chunkWorldY - 2 + seedY;
  int startZ = chunkWorldZ - 2 + seedZ;

  // 1. Cheese cave noise (seed offset 1000)
  FastNoiseGrid3D(data.cheeseNoise, startX, startY, startZ, SIZE, SIZE, SIZE,
                  cheeseScale, 1000);

  // 2. Spaghetti size modifier (seed offset 2000)
  FastNoiseGrid3D(data.spaghettiMod, startX, startY, startZ, SIZE, SIZE, SIZE,
                  spagModScale, 2000);

  // 3. Spaghetti noise 1 (seed offset 3000)
  FastNoiseGrid3D(data.spaghettiNoise1, startX, startY, startZ, SIZE, SIZE,
                  SIZE, spagNoiseScale, 3000);

  // 4. Spaghetti noise 2 with offset (seed offset 3000)
  FastNoiseGrid3D(data.spaghettiNoise2, startX + 100, startY + 98, startZ + 100,
                  SIZE, SIZE, SIZE, spagNoiseScale, 3000);

  // 5. Entrance noise (2D, seed offset 5000)
  FastNoiseGrid2D(data.entranceNoise, chunkWorldX - 2 + seedX,
                  chunkWorldZ - 2 + seedZ, SIZE, SIZE, 0.012f, 5000);
}

float WorldGenerator::GetBeachNoise(int gx, int gz) {
  if (!m_BeachNoise)
    InitializeFastNoise();

  int beachOffX = (m_Seed * 5432) % 65536;
  int beachOffZ = (m_Seed * 1234) % 65536;
  return m_BeachNoise->GenSingle2D((float)gx + beachOffX, (float)gz + beachOffZ,
                                   m_Seed);
}
