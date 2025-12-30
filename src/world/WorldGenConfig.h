#include <map>
#pragma once
#include <nlohmann/json.hpp>
#include <string>
#include <vector>

using json = nlohmann::json;

struct LandformConfigOverride {
  float baseHeight;
  float heightVariation;
  std::vector<float> octaveAmplitudes; // Overrides for 8 octaves
};

struct WorldGenConfig {
  int seed = 0;
  float globalScale = 1.0f;
  float terrainScale = 0.0025f;
  int seaLevel = 60;
  int surfaceDepth = 4;
  int worldHeight = 320; // World height in blocks (must be multiple of 32)

  // Noise Scales
  float tempScale = 0.003f;     // Balanced for moderate biome regions
  float humidityScale = 0.003f; // Balanced for moderate biome regions
  float landformScale =
      0.0015f; // Increased for more terrain variety (mountains, oceans, etc.)
  float climateScale = 0.0001f;
  float geologicScale = 0.001f;
  float biomeVariation = 0.25f; // Adds noise to break up smooth biome blobs
  float temperatureLapseRate = 0.006f; // Temperature decrease per block of
                                       // altitude (for snow-capped mountains)
  float geothermalGradient = 0.01f;    // Temperature increase per block of
                                       // depth (getting warmer underground)
  int lavaLevel = 10;                  // Depth at which caves fill with lava

  // Key is landform name: "oceans", "plains", "hills", "mountains", "valleys"
  std::map<std::string, LandformConfigOverride> landformOverrides;

  // Caves
  float caveFrequency = 0.015f;
  float caveThreshold = 0.55f;
  bool enableCaves = true;
  bool enableRavines = true;
  int ravineDepth = 40;
  float ravineWidth = 1.0f;
  float caveSize = 1.0f;
  float caveEntranceNoise = 0.2f;

  // Hydrology
  bool enableRivers = true;
  float riverScale = 0.005f;
  float riverThreshold = 0.08f;
  float riverDepth = 15.0f;
  int lakeLevel = 62;

  // Decorators
  bool enableOre = true;
  bool enableTrees = true;
  bool enableFlora = true;

  // Densities (per chunk or roll)
  int coalAttempts = 10;
  int ironAttempts = 5;
  float oakDensity = 5.0f;    // Roll %
  float pineDensity = 2.0f;   // Roll %
  float cactusDensity = 1.0f; // Roll %
  float floraDensity = 10.0f; // Grass roll %

  WorldGenConfig() {
    // Initialize with default values seen in WorldGenerator.cpp
    landformOverrides["oceans"] = {
        35.0f,
        40.0f,
        {0.60f, 0.20f, 0.10f, 0.05f, 0.025f, 0.012f, 0.006f, 0.003f}};
    landformOverrides["plains"] = {
        66.0f,
        15.0f,
        {0.55f, 0.28f, 0.14f, 0.07f, 0.035f, 0.018f, 0.009f, 0.0045f}};
    landformOverrides["hills"] = {
        72.0f,
        40.0f,
        {0.45f, 0.38f, 0.28f, 0.2f, 0.12f, 0.07f, 0.035f, 0.018f}};
    landformOverrides["mountains"] = {
        100.0f, 180.0f, {0.38f, 0.45f, 0.5f, 0.42f, 0.28f, 0.2f, 0.14f, 0.07f}};
    landformOverrides["valleys"] = {
        55.0f,
        20.0f,
        {0.65f, 0.22f, 0.11f, 0.055f, 0.028f, 0.014f, 0.007f, 0.0035f}};
  }
};

// nlohmann::json serialization
inline void to_json(json &j, const LandformConfigOverride &c) {
  j = json{{"baseHeight", c.baseHeight},
           {"heightVariation", c.heightVariation},
           {"octaveAmplitudes", c.octaveAmplitudes}};
}

inline void from_json(const json &j, LandformConfigOverride &c) {
  j.at("baseHeight").get_to(c.baseHeight);
  j.at("heightVariation").get_to(c.heightVariation);
  j.at("octaveAmplitudes").get_to(c.octaveAmplitudes);
}

inline void to_json(json &j, const WorldGenConfig &c) {
  j = json{{"seed", c.seed},
           {"globalScale", c.globalScale},
           {"terrainScale", c.terrainScale},
           {"seaLevel", c.seaLevel},
           {"surfaceDepth", c.surfaceDepth},
           {"worldHeight", c.worldHeight},
           {"tempScale", c.tempScale},
           {"humidityScale", c.humidityScale},
           {"landformScale", c.landformScale},
           {"climateScale", c.climateScale},
           {"geologicScale", c.geologicScale},
           {"biomeVariation", c.biomeVariation},
           {"temperatureLapseRate", c.temperatureLapseRate},
           {"geothermalGradient", c.geothermalGradient},
           {"lavaLevel", c.lavaLevel},
           {"landformOverrides", c.landformOverrides},
           {"caveFrequency", c.caveFrequency},
           {"caveThreshold", c.caveThreshold},
           {"enableCaves", c.enableCaves},
           {"enableRavines", c.enableRavines},
           {"ravineDepth", c.ravineDepth},
           {"ravineWidth", c.ravineWidth},
           {"caveSize", c.caveSize},
           {"caveEntranceNoise", c.caveEntranceNoise},
           {"enableRivers", c.enableRivers},
           {"riverScale", c.riverScale},
           {"riverThreshold", c.riverThreshold},
           {"riverDepth", c.riverDepth},
           {"lakeLevel", c.lakeLevel},
           {"enableOre", c.enableOre},
           {"enableTrees", c.enableTrees},
           {"enableFlora", c.enableFlora},
           {"coalAttempts", c.coalAttempts},
           {"ironAttempts", c.ironAttempts},
           {"oakDensity", c.oakDensity},
           {"pineDensity", c.pineDensity},
           {"cactusDensity", c.cactusDensity},
           {"floraDensity", c.floraDensity}};
}

inline void from_json(const json &j, WorldGenConfig &c) {
  j.at("seed").get_to(c.seed);
  j.at("globalScale").get_to(c.globalScale);
  j.at("terrainScale").get_to(c.terrainScale);
  j.at("seaLevel").get_to(c.seaLevel);
  j.at("surfaceDepth").get_to(c.surfaceDepth);
  j.at("worldHeight").get_to(c.worldHeight);
  j.at("tempScale").get_to(c.tempScale);
  j.at("humidityScale").get_to(c.humidityScale);
  j.at("landformScale").get_to(c.landformScale);
  j.at("climateScale").get_to(c.climateScale);
  j.at("geologicScale").get_to(c.geologicScale);
  j.at("biomeVariation").get_to(c.biomeVariation);
  j.at("temperatureLapseRate").get_to(c.temperatureLapseRate);
  j.at("geothermalGradient").get_to(c.geothermalGradient);
  j.at("lavaLevel").get_to(c.lavaLevel);
  j.at("landformOverrides").get_to(c.landformOverrides);
  j.at("caveFrequency").get_to(c.caveFrequency);
  j.at("caveThreshold").get_to(c.caveThreshold);
  j.at("enableCaves").get_to(c.enableCaves);
  j.at("enableRavines").get_to(c.enableRavines);
  j.at("ravineDepth").get_to(c.ravineDepth);
  j.at("ravineWidth").get_to(c.ravineWidth);
  j.at("caveSize").get_to(c.caveSize);
  j.at("caveEntranceNoise").get_to(c.caveEntranceNoise);
  j.at("enableRivers").get_to(c.enableRivers);
  j.at("riverScale").get_to(c.riverScale);
  j.at("riverThreshold").get_to(c.riverThreshold);
  j.at("riverDepth").get_to(c.riverDepth);
  j.at("lakeLevel").get_to(c.lakeLevel);
  j.at("enableOre").get_to(c.enableOre);
  j.at("enableTrees").get_to(c.enableTrees);
  j.at("enableFlora").get_to(c.enableFlora);
  j.at("coalAttempts").get_to(c.coalAttempts);
  j.at("ironAttempts").get_to(c.ironAttempts);
  j.at("oakDensity").get_to(c.oakDensity);
  j.at("pineDensity").get_to(c.pineDensity);
  j.at("cactusDensity").get_to(c.cactusDensity);
  j.at("floraDensity").get_to(c.floraDensity);
}
