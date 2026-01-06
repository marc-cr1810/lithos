#include "GlobalConfig.h"
#include "../debug/Logger.h"
#include <fstream>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

GlobalConfig &GlobalConfig::Get() {
  static GlobalConfig instance;
  return instance;
}

void GlobalConfig::Load(const std::string &path) {
  std::ifstream file(path);
  if (!file.is_open()) {
    LOG_ERROR("Failed to open global config: {}", path);
    return;
  }

  try {
    json j;
    file >> j;

    if (j.contains("saltwaterBlockCode"))
      j["saltwaterBlockCode"].get_to(saltwaterBlockCode);
    if (j.contains("waterBlockCode"))
      j["waterBlockCode"].get_to(waterBlockCode);
    if (j.contains("iceBlockCode"))
      j["iceBlockCode"].get_to(iceBlockCode);
    if (j.contains("basaltBlockCode"))
      j["basaltBlockCode"].get_to(basaltBlockCode);
    if (j.contains("lavaBlockCode"))
      j["lavaBlockCode"].get_to(lavaBlockCode);
    if (j.contains("mantleBlockCode"))
      j["mantleBlockCode"].get_to(mantleBlockCode);
    if (j.contains("defaultRockCode"))
      j["defaultRockCode"].get_to(defaultRockCode);

    LOG_INFO("Loaded global config from {}", path);
  } catch (const json::exception &e) {
    LOG_ERROR("JSON Parse Error in {}: {}", path, e.what());
  }
}
