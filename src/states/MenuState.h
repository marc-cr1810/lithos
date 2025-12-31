#pragma once

#include "../core/State.h"
#include "../world/WorldGenConfig.h"
#include <string>
#include <vector>

class MenuState : public State {
public:
  void Init(Application *app) override;
  void HandleInput(Application *app) override;
  void Update(Application *app, float dt) override;
  void Render(Application *app) override;
  void RenderUI(Application *app) override;
  void Cleanup() override;

private:
  void UpdatePreview();
  void SaveConfig(const std::string &name);
  void LoadConfig(const std::string &name);

  WorldGenConfig m_Config;
  char m_ConfigName[64] = "default_preset";
  char m_SeedBuffer[32];
  float m_PreviewData[128];
  float m_TempData[128];
  float m_HumidData[128];
  float m_BiomeData[128];
  float m_CaveProbData[128];
  float m_CaveSliceData[256 * 128]; // X: 256, Y: 128

  // Individual landform previews
  float m_OceansData[128];
  float m_ValleysData[128];
  float m_PlainsData[128];
  float m_HillsData[128];
  float m_MountainsData[128];

  // Visibility toggles for landforms
  bool m_ShowOceans = false;
  bool m_ShowValleys = false;
  bool m_ShowPlains = false;
  bool m_ShowHills = false;
  bool m_ShowMountains = false;
  bool m_ShowBlended = true;

  std::string m_BenchmarkResult;
  int m_PrevSeaLevel = 60; // Default startup value before config load
  bool m_ShouldOpenResults = false;
};
