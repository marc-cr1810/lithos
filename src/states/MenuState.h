#pragma once

#include "../core/State.h"
#include "../render/NoisePreview.h"
#include "../world/WorldGenConfig.h"
#include "../world/gen/NoiseManager.h"
#include <memory>
#include <string>
#include <vector>

#include "../render/Camera.h"
#include <glm/glm.hpp>

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
  bool m_ShowBlended = true; // Terrain height visible by default

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

  // Noise Previews
  std::unique_ptr<NoiseManager> m_PreviewNoiseManager;
  std::unique_ptr<NoisePreview> m_LandformPreview;
  std::unique_ptr<NoisePreview> m_EdgePreview;
  std::unique_ptr<NoisePreview> m_TerrainDetailPreview;
  std::unique_ptr<NoisePreview> m_TemperaturePreview;
  std::unique_ptr<NoisePreview> m_HumidityPreview;
  std::unique_ptr<NoisePreview> m_UpheavalPreview;
  std::unique_ptr<NoisePreview> m_GeologicPreview;
  bool m_ShowNoisePreviews = true;
  float m_PreviewUpdateTimer = 0.0f;
  bool m_NeedsPreviewUpdate = true;
  float m_NoisePreviewZoom = 1.0f; // 1.0 = 200px, 2.0 = 400px, etc.

  void InitPreview();
  void UpdatePreview3D();
  void UpdateNoisePreviews();
};
