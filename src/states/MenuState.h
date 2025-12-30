#pragma once

#include "../core/State.h"
#include "../world/WorldGenConfig.h"
#include <string>

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
};
