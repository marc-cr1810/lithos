#pragma once

#include "../core/State.h"
#include "../render/Shader.h"
#include "../render/Texture.h"
#include "../render/TextureAtlas.h"
#include "../world/World.h"
#include <entt/entt.hpp>
#include <glm/glm.hpp>
#include <memory>
#ifdef USE_GLEW
#include <GL/glew.h>
#endif
#include <GLFW/glfw3.h>

class GameState : public State {
public:
  GameState(glm::vec3 spawnPos);

  void Init(Application *app) override;
  void HandleInput(Application *app) override;
  void Update(Application *app, float dt) override;
  void Render(Application *app) override;
  void RenderUI(Application *app) override;
  void Cleanup() override;

private:
  void InitEntities(Application *app);
  void InitRendering(Application *app);

  // Member Variables
  glm::vec3 m_SpawnPos;

  // Rendering
  Shader *m_Shader = nullptr;
  TextureAtlas *m_Atlas = nullptr;
  Texture *m_BlockTexture = nullptr;

  unsigned int m_CrosshairVAO = 0, m_CrosshairVBO = 0;
  unsigned int m_SelectVAO = 0, m_SelectVBO = 0;

  // Game Logic
  bool m_IsPaused = false;
  bool m_IsDebugMode = false;
  bool m_ShowProfiler = false;
  entt::entity m_PlayerEntity;

  // Interaction
  BlockType m_SelectedBlock = STONE;
  uint8_t m_SelectedBlockMetadata = 0;
  bool m_FirstMouse = true;
  float m_LastX = 0, m_LastY = 0;

  // Timing
  float m_GlobalTime = 0.0f;
  float m_TickAccumulator = 0.0f;
  float m_SunStrength = 1.0f;

  // Raycasting
  glm::ivec3 m_HitPos;
  glm::ivec3 m_PrePos;
  bool m_Hit = false;

  // Debug UI Vars
  float m_DbgFrametimes[120] = {0};
  int m_DbgFrametimeOffset = 0;
  float m_DbgTeleportPos[3] = {0, 0, 0};
  bool m_DbgTimePaused = false;
  float m_DbgTimeSpeed = 1.0f;

  // New Debug Vars
  bool m_DbgChunkBorders = false;
  bool m_DbgUseHeatmap = false;
  bool m_DbgUseFog = false;
  float m_DbgFogDist = 50.0f;
  bool m_DbgFreezeCulling = false;
  bool m_DbgWireframe = false;
  glm::mat4 m_FrozenViewProj{1.0f};
  int m_DbgRenderedChunks = 0;
  bool m_DbgVsync = false;
  int m_DbgSimulationDistance = 4;
  int m_DbgRenderDistance = 8;
};
