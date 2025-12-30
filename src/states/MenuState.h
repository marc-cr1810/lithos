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
  WorldGenConfig m_Config;
  char m_SeedBuffer[32];
  float m_PreviewData[128];
  float m_TempData[128];
  float m_HumidData[128];
  int m_BiomeData[128];
  float m_CaveProbData[128];
  void UpdatePreview();
};
