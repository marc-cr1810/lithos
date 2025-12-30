#pragma once

#include "../core/State.h"
#include <glm/glm.hpp>

class LoadingState : public State {
public:
  void Init(Application *app) override;
  void HandleInput(Application *app) override;
  void Update(Application *app, float dt) override;
  void Render(Application *app) override;
  void RenderUI(Application *app) override;
  void Cleanup() override;

private:
  int m_SpawnX = 8;
  int m_SpawnZ = 8;
  float m_SpawnY = 85.0f;
  int m_SpawnRadius = 8; // Will be set based on debug/release

  bool m_FoundGround = false;
  double m_LoadingStartTime = 0.0;

  // Chunk Loading Tracking
  int m_LoadedCount = 0;
  int m_TotalChunksToCheck = 0;
};
