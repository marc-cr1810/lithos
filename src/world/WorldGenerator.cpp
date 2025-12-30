#include "WorldGenerator.h"
#include "../debug/Logger.h"
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

int WorldGenerator::GetHeight(int x, int z) {
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
    float blendedAmplitude =
        primary->octaveAmplitudes[i] * (1.0f - blendFactor) +
        secondary->octaveAmplitudes[i] * blendFactor;
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

  return (int)(blendedBaseHeight + noiseHeight * blendedVariation);
}

float WorldGenerator::GetTemperature(int x, int z) {
  // Very low frequency noise for large biomes
  int seedT = (seed * 555) % 65536;
  float nx = (float)x + (float)seedT;
  float nz = (float)z + (float)seedT;
  // Scale 0.001f means biomes are ~1000 blocks wide
  return glm::perlin(glm::vec2(nx, nz) * config.tempScale);
}

float WorldGenerator::GetHumidity(int x, int z) {
  int seedH = (seed * 888) % 65536;
  float nx = (float)x + (float)seedH;
  float nz = (float)z + (float)seedH;
  return glm::perlin(glm::vec2(nx, nz) * config.humidityScale);
}

Biome WorldGenerator::GetBiome(int x, int z) {
  float temp = GetTemperature(x, z);
  float humidity = GetHumidity(x, z);

  // Normalize logic slightly if needed, but perlin is approx -1 to 1

  if (temp > 0.3f) {
    // Hot
    if (humidity < -0.2f)
      return BIOME_DESERT; // Hot and Dry
    return BIOME_FOREST;   // Hot and Wet (Jungle-ish)
  } else if (temp < -0.3f) {
    // Cold
    if (humidity > 0.3f)
      return BIOME_TUNDRA; // Cold and snowy
    return BIOME_PLAINS;   // Cold Plains
  }

  // Moderate Temp
  if (humidity < -0.3f)
    return BIOME_PLAINS;
  return BIOME_FOREST;
}

BlockType WorldGenerator::GetSurfaceBlock(int gx, int gy, int gz,
                                          bool checkCarving) {
  int height = GetHeight(gx, gz);
  if (gy > height)
    return AIR;

  // Re-calculate climate data
  float temp = GetTemperature(gx, gz);
  float humidity = GetHumidity(gx, gz);

  BlockType surfaceBlock = GRASS;
  BlockType subsurfaceBlock = DIRT;

  if (temp > 0.3f) {
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
  int beachHeightLimit = 60 + (int)(beachNoise * 4.0f);
  BlockType beachBlock = (beachNoise > 0.4f) ? GRAVEL : SAND;

  BlockType type = AIR;
  if (gy == height) {
    if (gy < 60)
      type = (beachNoise > 0.0f) ? GRAVEL : DIRT;
    else if (gy <= beachHeightLimit)
      type = beachBlock;
    else
      type = surfaceBlock;
  } else if (gy > height - config.surfaceDepth) {
    if (gy < 60)
      type = DIRT;
    else
      type = subsurfaceBlock;
  } else {
    type = GetStrataBlock(gx, gy, gz);
  }

  // Cave/Ravine Carving Check
  if (checkCarving && type != AIR && type != WATER) {
    bool isUnderwater = (height <= 60);
    bool preserveCrust = false;

    // Consistency check with GenerateChunk carving logic
    if (isUnderwater && gy > height - 3)
      preserveCrust = true;
    if (gy <= 0)
      preserveCrust = true;

    if (!preserveCrust) {
      if (caveGenerator->IsCaveAt(gx, gy, gz, height) ||
          caveGenerator->IsRavineAt(gx, gy, gz, height)) {
        if (gy <= 10)
          return LAVA;
        else
          return AIR;
      }
    }
  }

  return type;
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
  for (int x = 0; x < CHUNK_SIZE; ++x) {
    for (int z = 0; z < CHUNK_SIZE; ++z) {
      int gx = cx * CHUNK_SIZE + x;
      int gz = cz * CHUNK_SIZE + z;
      column.heightMap[x][z] = GetHeight(gx, gz);
      column.biomeMap[x][z] = GetBiome(gx, gz);
    }
  }
}

void WorldGenerator::GenerateChunk(Chunk &chunk, const ChunkColumn &column) {
  glm::ivec3 pos = chunk.chunkPosition;

  for (int x = 0; x < CHUNK_SIZE; ++x) {
    for (int z = 0; z < CHUNK_SIZE; ++z) {
      // Global Coordinates
      int gx = pos.x * CHUNK_SIZE + x;
      int gz = pos.z * CHUNK_SIZE + z;

      // Get Height from column
      int height = column.heightMap[x][z];
      bool isUnderwater = (height <= config.seaLevel);

      for (int y = 0; y < CHUNK_SIZE; ++y) {
        int gy = pos.y * CHUNK_SIZE + y;
        BlockType type = GetSurfaceBlock(gx, gy, gz);

        // 1. Water Fill
        if (type == AIR && gy <= config.seaLevel) {
          // Ice forms in cold climates
          float temp = GetTemperature(gx, gz);
          if (temp < -0.3f && gy == config.seaLevel)
            type = ICE;
          else
            type = WATER;
        }

        // 2. Carve Caves using new CaveGenerator
        bool preserveCrust = false;
        // Only try to carve if the block is solid (Not Air, Not Water)
        // Only preserve crust underwater to prevent ocean draining
        if (isUnderwater && gy > height - 3)
          preserveCrust = true;

        // Bedrock preservation
        if (gy <= 0)
          preserveCrust = true;

        if (!preserveCrust) {
          // Use new 3D cave generation and ravines
          bool shouldCarve = false;
          if (config.enableCaves && caveGenerator->IsCaveAt(gx, gy, gz, height))
            shouldCarve = true;
          if (config.enableRavines &&
              caveGenerator->IsRavineAt(gx, gy, gz, height))
            shouldCarve = true;

          if (shouldCarve) {
            // Cave Air or Lava?
            if (gy <= 10)
              type = LAVA;
            else
              type = AIR;
          }
        }

        // 3. Bedrock Flattening (Roughness)
        // Only apply if the block hasn't been carved by a cave or ravine
        if (gy <= 4 && type != AIR && type != LAVA) {
          if (gy > 0) {
            int chance = 100 - (gy * 20);
            if ((rand() % 100) < chance)
              type = GetSurfaceBlock(gx, gy, gz);
          }
        }

        chunk.setBlock(x, y, z, type);

        // 4. Post-Set Fixes (Grass->Dirt under water)
        if (type == WATER && gy <= config.seaLevel) {
          if (y > 0) {
            if (chunk.getBlock(x, y - 1, z).getType() == GRASS) {
              chunk.setBlock(x, y - 1, z, DIRT);
            }
          }
        }
      }
    }
  }

  // Apply Decorators
  for (auto d : decorators) {
    d->Decorate(chunk, *this, column);
  }
}
