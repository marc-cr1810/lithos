#pragma once

#include "../core/State.h"
#include "../world/WorldGenConfig.h"
#include <memory>
#include <nlohmann/json.hpp>
#include <string>
#include <vector>


#include "../render/Camera.h"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

class MenuState : public State {
public:
  MenuState();
  virtual ~MenuState();
  void Init(Application *app) override;
  void HandleInput(Application *app) override;
  void Update(Application *app, float dt) override;
  void Render(Application *app) override;
  void RenderUI(Application *app) override;
  void Cleanup() override;

private:
  void SaveConfig(const std::string &name);
  void LoadConfig(const std::string &name);

  WorldGenConfig m_Config;
  char m_ConfigName[64] = "default_preset";
  char m_SeedBuffer[32];

  std::string m_BenchmarkResult;
  int m_BenchmarkSize = 4; // Side length for NxN benchmark (e.g. 4x4)
  int m_PrevSeaLevel = 60; // Default startup value before config load
  bool m_ShouldOpenResults = false;
  bool m_IsBenchmarkResultsOpen = false;

  // 3D Preview
  std::unique_ptr<class World> m_PreviewWorld;
  std::vector<std::shared_ptr<class Chunk>> m_BenchmarkChunks;
  std::unique_ptr<class Framebuffer> m_PreviewFBO;
  class Shader *m_PreviewShader = nullptr;
  class TextureAtlas *m_PreviewAtlas = nullptr;
  glm::vec3 m_PreviewTarget{0.0f, 80.0f, 0.0f}; // Target for camera to look at
  class Texture *m_PreviewTexture = nullptr;
  class Camera m_PreviewCamera;
  float m_PreviewYaw = -45.0f;
  float m_PreviewPitch = -30.0f;
  float m_PreviewDistance = 80.0f;
  glm::vec2 m_LastMousePos = {0.0f, 0.0f};
  bool m_IsDraggingPreview = false;

  void InitPreview();
  void UpdatePreview3D();
};
