#pragma once
#include <string>
#include <string_view>

struct GlobalConfig {
  std::string saltwaterBlockCode = "lithos:water";
  std::string waterBlockCode = "lithos:water";
  std::string iceBlockCode = "lithos:ice";
  std::string basaltBlockCode = "lithos:rock_basalt";
  std::string lavaBlockCode = "lithos:lava";
  std::string mantleBlockCode = "lithos:mantle";
  std::string defaultRockCode = "lithos:rock_granite";

  static GlobalConfig &Get();
  void Load(const std::string &path);
};
