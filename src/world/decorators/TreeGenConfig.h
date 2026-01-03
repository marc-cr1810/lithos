#pragma once
#include "TreeStructure.h"
#include <nlohmann/json.hpp>
#include <string>
#include <vector>

using json = nlohmann::json;

struct TreeGenerator {
  std::string generator; // Tree type name (config file basename)

  float weight = 100.0f;
  float minTemp = -999.0f, maxTemp = 999.0f;
  float minRain = -999.0f, maxRain = 999.0f;
  float minFert = -999.0f, maxFert = 999.0f;
  float minForest = -999.0f, maxForest = 999.0f;
  float minHeight = 0.0f, maxHeight = 1.0f;

  float minSize = 1.0f;
  float maxSize = 1.0f;
  float suitabilitySizeBonus = 0.0f;

  std::string habitat = "Land";

  // Calculate suitability score
  bool IsSuitable(float temp, float rain, float fert, float forest,
                  float heightNormalized) const {
    if (temp < minTemp || temp > maxTemp)
      return false;
    if (rain < minRain || rain > maxRain)
      return false;
    if (fert < minFert || fert > maxFert)
      return false;
    if (forest < minForest || forest > maxForest)
      return false;
    if (heightNormalized < minHeight || heightNormalized > maxHeight)
      return false;

    return true;
  }
};

inline void from_json(const json &j, TreeGenerator &tg) {
  if (j.contains("generator"))
    tg.generator = j.at("generator").get<std::string>();
  if (j.contains("weight"))
    tg.weight = j.at("weight").get<float>();

  if (j.contains("minTemp"))
    tg.minTemp = j.at("minTemp").get<float>();
  if (j.contains("maxTemp"))
    tg.maxTemp = j.at("maxTemp").get<float>();
  if (j.contains("minRain"))
    tg.minRain = j.at("minRain").get<float>();
  if (j.contains("maxRain"))
    tg.maxRain = j.at("maxRain").get<float>();
  if (j.contains("minFert"))
    tg.minFert = j.at("minFert").get<float>();
  if (j.contains("maxFert"))
    tg.maxFert = j.at("maxFert").get<float>();
  if (j.contains("minForest"))
    tg.minForest = j.at("minForest").get<float>();
  if (j.contains("maxForest"))
    tg.maxForest = j.at("maxForest").get<float>();
  if (j.contains("minHeight"))
    tg.minHeight = j.at("minHeight").get<float>();
  if (j.contains("maxHeight"))
    tg.maxHeight = j.at("maxHeight").get<float>();

  if (j.contains("minSize"))
    tg.minSize = j.at("minSize").get<float>();
  if (j.contains("maxSize"))
    tg.maxSize = j.at("maxSize").get<float>();
  if (j.contains("suitabilitySizeBonus"))
    tg.suitabilitySizeBonus = j.at("suitabilitySizeBonus").get<float>();

  if (j.contains("habitat"))
    tg.habitat = j.at("habitat").get<std::string>();
}

struct TreeGenConfig {
  Distribution treesPerChunk;
  Distribution shrubsPerChunk;

  std::string vinesBlockGenerator;
  std::string vinesBlockCodeEnd;
  float vinesMinRain = 0.0f;
  float vinesMinTemp = 0.0f;

  std::vector<TreeGenerator> treegens;
  std::vector<TreeGenerator> shrubgens;
};

inline void from_json(const json &j, TreeGenConfig &c) {
  if (j.contains("treesPerChunk"))
    c.treesPerChunk = j.at("treesPerChunk");
  if (j.contains("shrubsPerChunk"))
    c.shrubsPerChunk = j.at("shrubsPerChunk");

  if (j.contains("vinesBlockGenerator"))
    c.vinesBlockGenerator = j.at("vinesBlockGenerator").get<std::string>();
  if (j.contains("vinesBlockCodeEnd"))
    c.vinesBlockCodeEnd = j.at("vinesBlockCodeEnd").get<std::string>();
  if (j.contains("vinesMinRain"))
    c.vinesMinRain = j.at("vinesMinRain").get<float>();
  if (j.contains("vinesMinTemp"))
    c.vinesMinTemp = j.at("vinesMinTemp").get<float>();

  if (j.contains("treegens"))
    c.treegens = j.at("treegens").get<std::vector<TreeGenerator>>();
  if (j.contains("shrubgens"))
    c.shrubgens = j.at("shrubgens").get<std::vector<TreeGenerator>>();
}
