#include "MenuState.h"
#include "../core/Application.h"
#include "../debug/Benchmark.h"
#include "../debug/Logger.h"
#include "../render/Framebuffer.h"
#include "../render/Shader.h"
#include "../render/Texture.h"
#include "../render/TextureAtlas.h"
#include "../world/Block.h" // For BlockRegistry
#include "../world/World.h"
#include "../world/WorldGenerator.h"
#include "GameState.h"
#include "LoadingState.h"
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include <GLFW/glfw3.h>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <glm/gtc/matrix_transform.hpp>

MenuState::MenuState() = default;
MenuState::~MenuState() = default;

// Helper to display a little (?) mark which shows a tooltip when hovered.
static void HelpMarker(const char *desc) {
  ImGui::SameLine();
  ImGui::TextDisabled("(?)");
  if (ImGui::IsItemHovered()) {
    ImGui::BeginTooltip();
    ImGui::PushTextWrapPos(ImGui::GetFontSize() * 35.0f);
    ImGui::TextUnformatted(desc);
    ImGui::PopTextWrapPos();
    ImGui::EndTooltip();
  }
}

void MenuState::InitPreview() {
  m_PreviewFBO = std::make_unique<Framebuffer>(512, 512);

  // Initialize Camera
  m_PreviewCamera = Camera(glm::vec3(0.0f, 140.0f, 80.0f),
                           glm::vec3(0.0f, 1.0f, 0.0f), -45.0f, -30.0f);
  m_PreviewDistance = 80.0f;
  m_PreviewYaw = -45.0f;   // 45 degrees
  m_PreviewPitch = -30.0f; // Look down slightly

  // Initialize Resources (Shader & Atlas) from ResourceManager
  auto &rm = ResourceManager::Get();

  m_PreviewShader = rm.GetShader("basic");
  // If shader failed to load in RM, this might be null. We should check or
  // handle it. Assuming RM logs errors and returns nullptr.

  // Atlas
  m_PreviewAtlas = rm.GetTextureAtlas("blocks");

  // Texture
  // We added code to RM to create texture "blocks" when loading atlas "blocks".
  m_PreviewTexture = rm.GetTexture("blocks");

  if (m_PreviewShader) {
    m_PreviewShader->use();
    m_PreviewShader->setInt("texture1", 0);
    if (m_PreviewAtlas) {
      // Calculate uvScale
      float uScale = 16.0f / m_PreviewAtlas->GetWidth();
      float vScale = 16.0f / m_PreviewAtlas->GetHeight();
      m_PreviewShader->setVec2("uvScale", uScale, vScale);
    }
  }

  // Initial World Update
  UpdatePreview3D();
}

void MenuState::UpdatePreview3D() {
  bool useBenchmark = !m_BenchmarkChunks.empty();
  if (!m_PreviewWorld || m_PreviewWorld->worldSeed != m_Config.seed ||
      useBenchmark) {
    m_PreviewWorld = std::make_unique<World>(m_Config);

    if (useBenchmark) {
      for (auto &c : m_BenchmarkChunks) {
        m_PreviewWorld->insertChunk(c);
      }

      // Focus on center of benchmark bounds
      int minX = 100000, maxX = -100000;
      int minZ = 100000, maxZ = -100000;

      for (const auto &c : m_BenchmarkChunks) {
        if (c->chunkPosition.x < minX)
          minX = c->chunkPosition.x;
        if (c->chunkPosition.x > maxX)
          maxX = c->chunkPosition.x;
        if (c->chunkPosition.z < minZ)
          minZ = c->chunkPosition.z;
        if (c->chunkPosition.z > maxZ)
          maxZ = c->chunkPosition.z;
      }

      // Geometric center: Average of min edge and max edge
      // Min edge x: minX * CHUNK_SIZE
      // Max edge x: (maxX + 1) * CHUNK_SIZE
      float size = (float)CHUNK_SIZE;
      float cx = (minX * size + (maxX + 1) * size) / 2.0f;
      float cz = (minZ * size + (maxZ + 1) * size) / 2.0f;

      m_PreviewTarget = glm::vec3(cx, (float)m_Config.seaLevel, cz);

    } else {
      // Default target if live previewing
      m_PreviewTarget = glm::vec3(0.0f, 80.0f, 0.0f);
    }

    // Calculate ViewProjection for loading chunks
    // Use the same logic as Render() to ensure Frustum Culling allows these
    // chunks
    float aspect = 1.0f; // Default square assumed for loading context
    if (m_PreviewFBO)
      aspect = (float)m_PreviewFBO->m_Width / (float)m_PreviewFBO->m_Height;

    // Note: Render() will use m_PreviewTarget for camera position.
    // Here we need a temporary view matrix using the same target to ensure
    // loadChunks (if called) uses the correct frustum.

    glm::vec3 target = m_PreviewTarget;
    float camX = sin(glm::radians(m_PreviewYaw)) * m_PreviewDistance *
                 cos(glm::radians(m_PreviewPitch));
    float camY = -sin(glm::radians(m_PreviewPitch)) * m_PreviewDistance;
    float camZ = cos(glm::radians(m_PreviewYaw)) * m_PreviewDistance *
                 cos(glm::radians(m_PreviewPitch));
    glm::vec3 pos = target + glm::vec3(camX, camY, camZ);

    glm::mat4 view = glm::lookAt(pos, target, glm::vec3(0, 1, 0));
    glm::mat4 projection =
        glm::perspective(glm::radians(45.0f), aspect, 0.1f, 1000.0f);

    if (!useBenchmark) {
      m_PreviewWorld->loadChunks(glm::vec3(0, 0, 0), 4, projection * view);
    }
  }
}

void MenuState::Init(Application *app) {
  glfwSetInputMode(app->GetWindow(), GLFW_CURSOR, GLFW_CURSOR_NORMAL);

  // Inherit seed from app config (allows CLI to work)
  m_Config.seed = app->GetConfig().seed;
  m_ConfigName[0] = '\0';
  LoadConfig("default"); // Try load default

  // Pre-calculate benchmarks or similar? No.

  // Init Preview
  InitPreview();

  // Initialize Noise Previews
  m_PreviewNoiseManager = std::make_unique<NoiseManager>(m_Config);
  m_LandformPreview = std::make_unique<NoisePreview>(256, 256);
  m_EdgePreview = std::make_unique<NoisePreview>(256, 256);
  m_TerrainDetailPreview = std::make_unique<NoisePreview>(256, 256);
  m_TemperaturePreview = std::make_unique<NoisePreview>(256, 256);
  m_HumidityPreview = std::make_unique<NoisePreview>(256, 256);
  m_UpheavalPreview = std::make_unique<NoisePreview>(256, 256);
  m_GeologicPreview = std::make_unique<NoisePreview>(256, 256);
  UpdateNoisePreviews();

  // Initial seed buffer
  snprintf(m_SeedBuffer, sizeof(m_SeedBuffer), "%d", m_Config.seed);

  // Initialize landform overrides with default amplitudes if not already
  // present
  auto initLandform = [&](std::string name, float base, float var,
                          std::vector<float> amps) {
    if (m_Config.landformOverrides.find(name) ==
        m_Config.landformOverrides.end()) {
      m_Config.landformOverrides[name] = {base, var, amps};
    }
  };

  initLandform("oceans", 35.0f, 40.0f,
               {0.60f, 0.20f, 0.10f, 0.05f, 0.025f, 0.012f, 0.006f, 0.003f,
                0.015f, 0.0008f});
  initLandform("plains", 70.0f, 15.0f,
               {0.55f, 0.28f, 0.14f, 0.07f, 0.035f, 0.018f, 0.009f, 0.0045f,
                0.0022f, 0.0011f});
  initLandform("hills", 75.0f, 40.0f,
               {0.45f, 0.38f, 0.28f, 0.2f, 0.12f, 0.07f, 0.035f, 0.018f, 0.009f,
                0.0045f});
  initLandform(
      "mountains", 100.0f, 120.0f,
      {0.38f, 0.45f, 0.5f, 0.42f, 0.28f, 0.2f, 0.14f, 0.07f, 0.035f, 0.018f});
  initLandform("valleys", 62.0f, 20.0f,
               {0.65f, 0.22f, 0.11f, 0.055f, 0.028f, 0.014f, 0.007f, 0.0035f,
                0.017f, 0.0008f});

  UpdatePreview();
}

void MenuState::UpdatePreview() {
  // Create temporary WorldGenerator with current config
  WorldGenerator tempGen(m_Config);

  // Sample 128 points along X-axis (0 to 512 blocks)
  for (int i = 0; i < 128; i++) {
    int x = i * 4; // Sample every 4 blocks
    int z = 0;     // Along Z=0 line

    // Get blended final height
    m_PreviewData[i] = (float)tempGen.GetHeight(x, z);

    // Get landform name to determine which landform is dominant
    std::string landform = tempGen.GetLandformNameAt(x, z);

    // Store landform ID for cellular visualization
    if (landform == "oceans") {
      m_BiomeData[i] = 0.0f;
      m_OceansData[i] = m_PreviewData[i];
    } else if (landform == "valleys") {
      m_BiomeData[i] = 1.0f;
      m_ValleysData[i] = m_PreviewData[i];
    } else if (landform == "plains") {
      m_BiomeData[i] = 2.0f;
      m_PlainsData[i] = m_PreviewData[i];
    } else if (landform == "hills") {
      m_BiomeData[i] = 3.0f;
      m_HillsData[i] = m_PreviewData[i];
    } else if (landform == "mountains") {
      m_BiomeData[i] = 4.0f;
      m_MountainsData[i] = m_PreviewData[i];
    }

    // Get temperature & humidity for reference
    m_TempData[i] = tempGen.GetTemperature(x, z);
    m_HumidData[i] = tempGen.GetHumidity(x, z);
  }

  // Clear any benchmark results if we are live-editing
  m_BenchmarkChunks.clear();
  UpdatePreview3D();
}

void MenuState::HandleInput(Application *app) {}

void MenuState::Update(Application *app, float dt) {
  // Debounced noise preview updates
  if (m_NeedsPreviewUpdate) {
    m_PreviewUpdateTimer += dt;
    if (m_PreviewUpdateTimer >= 0.5f) { // 500ms debounce
      UpdateNoisePreviews();
      m_PreviewUpdateTimer = 0.0f;
    }
  } else {
    m_PreviewUpdateTimer = 0.0f;
  }
}

void MenuState::Render(Application *app) {
  int width, height;
  glfwGetFramebufferSize(app->GetWindow(), &width, &height);
  glViewport(0, 0, width, height);
  glClearColor(0.05f, 0.05f, 0.1f, 1.0f);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

  // 1. Update Preview World (Meshing etc)
  if (m_IsBenchmarkResultsOpen && m_PreviewWorld) {
    m_PreviewWorld->Update(); // Main thread update (Mesh Uploads)
    // Re-trigger load to ensure chunks are kept loaded
    // (Though loadChunks does checks, it might be fine)
  }

  // 2. Render to FBO
  if (m_IsBenchmarkResultsOpen && m_PreviewFBO && m_PreviewWorld) {
    m_PreviewFBO->Bind();
    glEnable(GL_DEPTH_TEST);              // Enable depth test for 3D rendering
    glClearColor(0.5f, 0.7f, 1.0f, 1.0f); // Sky color
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // Camera Orbit Logic
    float camX = sin(glm::radians(m_PreviewYaw)) * m_PreviewDistance *
                 cos(glm::radians(m_PreviewPitch));
    float camY = -sin(glm::radians(m_PreviewPitch)) * m_PreviewDistance;
    float camZ = cos(glm::radians(m_PreviewYaw)) * m_PreviewDistance *
                 cos(glm::radians(m_PreviewPitch));

    glm::vec3 target = m_PreviewTarget;
    m_PreviewCamera.Position = target + glm::vec3(camX, camY, camZ);
    m_PreviewCamera.Yaw = m_PreviewYaw + 180.0f; // Look back at center
    m_PreviewCamera.Pitch = m_PreviewPitch;

    glm::mat4 view =
        glm::lookAt(m_PreviewCamera.Position, target, glm::vec3(0, 1, 0));

    // Fix perspective call using correct FBO aspect
    float aspect = (float)m_PreviewFBO->m_Width / (float)m_PreviewFBO->m_Height;
    if (aspect < 0.1f)
      aspect = 0.1f;
    glm::mat4 projection =
        glm::perspective(glm::radians(45.0f), aspect, 0.1f, 5000.0f);

    // Continuous chunk loading for preview (ONLY if not viewing fixed benchmark
    // result)
    if (m_BenchmarkChunks.empty()) {
      m_PreviewWorld->loadChunks(target, 4, projection * view);
    }

    // Use local shader
    m_PreviewShader->use();
    m_PreviewShader->setMat4("view", view);
    m_PreviewShader->setMat4("projection", projection);
    m_PreviewShader->setVec3("viewPos", m_PreviewCamera.Position);
    m_PreviewShader->setFloat("fogDist", 5000.0f); // Large fog for preview
    m_PreviewShader->setBool("useTexture", true);

    // Bind Texture
    glActiveTexture(GL_TEXTURE0);
    if (m_PreviewTexture)
      m_PreviewTexture->bind();

    // Render World
    // Use large render distance for preview (256 chunks radius covers ~8000
    // blocks)
    m_PreviewWorld->render(*m_PreviewShader, projection * view,
                           m_PreviewCamera.Position, 256);

    glDisable(GL_DEPTH_TEST);
    m_PreviewFBO->Unbind();
  }
}

void MenuState::RenderUI(Application *app) {
  ImGui_ImplOpenGL3_NewFrame();
  ImGui_ImplGlfw_NewFrame();
  ImGui::NewFrame();

  ImGuiViewport *viewport = ImGui::GetMainViewport();
  ImGui::SetNextWindowPos(viewport->Pos);
  ImGui::SetNextWindowSize(viewport->Size);

  if (ImGui::Begin("World Configuration", nullptr,
                   ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                       ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse)) {
    // Seed input - visible on all tabs
    ImGui::Text("World Seed:");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(150);
    bool changed = false;
    if (ImGui::InputText("##seed", m_SeedBuffer, sizeof(m_SeedBuffer),
                         ImGuiInputTextFlags_CharsDecimal)) {
      m_Config.seed = atoi(m_SeedBuffer);
      changed = true;
    }
    ImGui::SameLine();
    if (ImGui::Button("Randomize##seed")) {
      m_Config.seed = rand();
      snprintf(m_SeedBuffer, sizeof(m_SeedBuffer), "%d", m_Config.seed);
      changed = true;
    }
    if (changed)
      UpdatePreview();

    ImGui::Separator();

    // Start Game button at the top for easy access
    if (ImGui::Button("Start Game", ImVec2(-1, 40))) {
      app->ChangeState(std::make_unique<LoadingState>(m_Config));
    }

    ImGui::Separator();

    if (ImGui::BeginTabBar("ConfigTabs")) {
      // General Tab
      if (ImGui::BeginTabItem("General")) {
        ImGui::Dummy(ImVec2(0, 5));
        ImGui::Text("Basic World Settings");
        ImGui::Separator();

        bool changed = false;
        if (ImGui::InputText("Seed", m_SeedBuffer, sizeof(m_SeedBuffer))) {
          m_Config.seed = atoi(m_SeedBuffer);
          changed = true;
        }
        HelpMarker(
            "The seed used for noise generation. Same seed = same world.");
        ImGui::SameLine();
        if (ImGui::Button("Randomize")) {
          m_Config.seed = rand();
          snprintf(m_SeedBuffer, sizeof(m_SeedBuffer), "%d", m_Config.seed);
          changed = true;
        }

        int oldSeaLevel = m_Config.seaLevel;
        if (ImGui::SliderInt("Sea Level", &m_Config.seaLevel, 0,
                             m_Config.worldHeight - 1)) {
          int diff = m_Config.seaLevel - oldSeaLevel;
          // Shift all landforms by differences to maintain relative height
          for (auto &kv : m_Config.landformOverrides) {
            kv.second.baseHeight += (float)diff;
          }
          changed = true;
        }
        HelpMarker("The Y-level where water fills up to.");
        if (ImGui::SliderFloat("Terrain Scale", &m_Config.terrainScale, 0.0001f,
                               0.01f))
          changed = true;
        HelpMarker("Controls the 'zoom' level of the terrain noise. Lower = "
                   "larger features.");
        if (ImGui::SliderInt("Surface Depth", &m_Config.surfaceDepth, 1, 10))
          changed = true;
        HelpMarker(
            "How many blocks of 'surface' material (dirt/sand) are above "
            "the stone strata.");
        if (ImGui::SliderFloat("Global Scale", &m_Config.globalScale, 0.1f,
                               5.0f))
          changed = true;
        HelpMarker("Multiplier for all terrain heights.");

        // World Height slider with validation - use step size of 32
        if (ImGui::SliderInt("World Height", &m_Config.worldHeight, 32, 1024,
                             "%d blocks")) {
          // Ensure it's a multiple of 32
          m_Config.worldHeight = ((m_Config.worldHeight + 16) / 32) * 32;
          if (m_Config.worldHeight < 32)
            m_Config.worldHeight = 32;
          if (m_Config.worldHeight > 1024)
            m_Config.worldHeight = 1024;

          // Clamp sea level to be within world height
          if (m_Config.seaLevel >= m_Config.worldHeight) {
            m_Config.seaLevel = m_Config.worldHeight / 2;
          }
          changed = true;
        }
        HelpMarker("Maximum world height in blocks. Snaps to multiples of 32 "
                   "(chunk size).");

        // Fixed World Settings
        if (ImGui::Checkbox("Fixed World Size", &m_Config.fixedWorld))
          changed = true;
        HelpMarker("If enabled, the world has a fixed finite size and terrain "
                   "is pre-generated (faster runtime, slower startup).");

        if (m_Config.fixedWorld) {
          if (ImGui::SliderInt("Map Size", &m_Config.fixedWorldSize, 128, 4096,
                               "%d blocks"))
            changed = true;
          HelpMarker("Size of the fixed world (Square). Larger maps take "
                     "longer to generate at start.");
        }

        ImGui::Dummy(ImVec2(0, 10));
        ImGui::Text("Benchmarking");
        ImGui::Separator();
        ImGui::SliderInt("Benchmark Area Size", &m_BenchmarkSize, 1, 16,
                         "%d columns (Square)");
        HelpMarker(
            "Size of the area to generate for benchmarking. Larger sizes "
            "provide more stable averages but take longer.");

        if (ImGui::Button("Run Benchmark")) {
          // Close config window to let benchmark run (optional, but cleaner)
          // Actually, we show a popup, so it's fine.
          StartBenchmarkAsync(m_Config, m_BenchmarkSize);
          ImGui::OpenPopup("Running Benchmark...");
        }

        ImGuiViewport *viewport = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(viewport->GetCenter(), ImGuiCond_Appearing,
                                ImVec2(0.5f, 0.5f));
        if (ImGui::BeginPopupModal("Running Benchmark...", NULL,
                                   ImGuiWindowFlags_AlwaysAutoResize |
                                       ImGuiWindowFlags_NoMove)) {
          BenchmarkStatus &status = GetBenchmarkStatus();
          float progress = status.progress;
          ImGui::Text("Generating chunks... %.0f%%", progress * 100.0f);
          ImGui::ProgressBar(progress, ImVec2(300, 0));

          if (status.isFinished) {
            // Benchmark complete, transition to result
            std::lock_guard<std::mutex> lock(status.resultMutex);
            BenchmarkResult res = status.result;

            std::string msg =
                "Total Time: " + std::to_string(res.totalTimeMs) + " ms\n" +
                "Chunks: " + std::to_string(res.chunksGenerated) + "\n" +
                "Avg/Chunk: " + std::to_string(res.avgChunkTimeMs) + " ms\n\n";

            msg += "Breakdown (Avg per Chunk):\n";
            for (const auto &kv : res.stepAvgTimes) {
              msg +=
                  " - " + kv.first + ": " + std::to_string(kv.second) + " ms\n";
            }
            m_BenchmarkResult = msg;
            m_BenchmarkChunks = res.generatedChunks;

            // Log to console for easy copy/paste
            spdlog::info("=== Benchmark Results ===");
            spdlog::info("{}", msg);
            spdlog::info("========================");

            ImGui::CloseCurrentPopup();
            m_ShouldOpenResults = true;
            m_IsBenchmarkResultsOpen = true;
            UpdatePreview3D(); // Generate preview for the benchmarked config
          }

          ImGui::EndPopup();
        }

        if (m_ShouldOpenResults) {
          ImGui::OpenPopup("Benchmark Results");
          m_ShouldOpenResults = false;
        }

        ImGui::SetNextWindowPos(viewport->GetCenter(), ImGuiCond_Appearing,
                                ImVec2(0.5f, 0.5f));
        if (ImGui::BeginPopupModal("Benchmark Results", NULL,
                                   ImGuiWindowFlags_AlwaysAutoResize)) {
          ImGui::Text("%s", m_BenchmarkResult.c_str());
          ImGui::Separator();

          // 3D Preview inside Result Popup
          if (m_PreviewFBO) {
            // Force a size for now to avoid layout loops in auto-resize popup.
            ImVec2 size(512, 384);
            if (size.x != m_PreviewFBO->m_Width ||
                size.y != m_PreviewFBO->m_Height) {
              m_PreviewFBO->Resize((int)size.x, (int)size.y);
            }

            ImGui::Image((void *)(intptr_t)m_PreviewFBO->GetTextureID(), size,
                         ImVec2(0, 1), ImVec2(1, 0));
            bool isHovered = ImGui::IsItemHovered();

            // Debug Overlay: Camera Info
            ImVec2 overlayPos = ImGui::GetItemRectMin();
            ImVec2 restorePos =
                ImGui::GetCursorScreenPos(); // Save position for next controls

            ImGui::SetCursorScreenPos(
                ImVec2(overlayPos.x + 5, overlayPos.y + 5));
            ImGui::TextColored(
                ImVec4(1, 1, 0, 1),
                "Cam: Yaw %.1f, Pitch %.1f, Dist %.1f\nChunks: %zu",
                m_PreviewYaw, m_PreviewPitch, m_PreviewDistance,
                m_PreviewWorld ? m_PreviewWorld->getChunkCount() : 0);

            // Restore cursor for next items (Separator, Buttons)
            ImGui::SetCursorScreenPos(restorePos);

            // Input Handling for Orbit
            if (isHovered) {
              if (ImGui::IsMouseDragging(ImGuiMouseButton_Right)) {
                ImVec2 delta = ImGui::GetMouseDragDelta(ImGuiMouseButton_Right);
                m_PreviewYaw += delta.x * 0.5f;
                m_PreviewPitch += delta.y * 0.5f;
                if (m_PreviewPitch > 89.0f)
                  m_PreviewPitch = 89.0f;
                if (m_PreviewPitch < -89.0f)
                  m_PreviewPitch = -89.0f;
                ImGui::ResetMouseDragDelta(ImGuiMouseButton_Right);
              }
              // Zoom
              float wheel = ImGui::GetIO().MouseWheel;
              if (wheel != 0) {
                m_PreviewDistance -= wheel * 5.0f;
                if (m_PreviewDistance < 10.0f)
                  m_PreviewDistance = 10.0f;
                // Dynamic max distance based on grid size
                float maxDist =
                    (float)m_BenchmarkSize * (float)CHUNK_SIZE * 2.5f;
                if (maxDist < 200.0f)
                  maxDist = 200.0f;

                if (m_PreviewDistance > maxDist)
                  m_PreviewDistance = maxDist;
              }
            }
            ImGui::Separator();
          }

          if (ImGui::Button("Copy to Clipboard", ImVec2(150, 0))) {
            ImGui::SetClipboardText(m_BenchmarkResult.c_str());
          }
          ImGui::SameLine();
          if (ImGui::Button("OK", ImVec2(120, 0))) {
            m_IsBenchmarkResultsOpen = false;
            ImGui::CloseCurrentPopup();
          }
          ImGui::EndPopup();
        }

        // Performance warning for high values

        // Performance warning for high values
        if (m_Config.worldHeight > 512) {
          ImGui::TextColored(
              ImVec4(1.0f, 0.6f, 0.0f, 1.0f),
              "WARNING: Heights above 512 may impact performance!");
        }

        if (changed)
          UpdatePreview();
        ImGui::EndTabItem();
      }

      // Climate Tab
      if (ImGui::BeginTabItem("Climate")) {
        ImGui::Dummy(ImVec2(0, 5));

        ImGui::Text("Climate & Temperature Settings");
        ImGui::Separator();
        bool changed = false;
        if (ImGui::SliderFloat("Temp Scale", &m_Config.tempScale, 0.0001f,
                               0.01f))
          changed = true;
        HelpMarker("Scale of temperature variation. Controls biome size.");
        if (ImGui::SliderFloat("Humidity Scale", &m_Config.humidityScale,
                               0.0001f, 0.01f))
          changed = true;
        HelpMarker("Scale of rainfall variation. Affects vegetation.");

        if (ImGui::SliderFloat("Biome Variation", &m_Config.biomeVariation,
                               0.0f, 0.5f))
          changed = true;
        HelpMarker(
            "Adds noise to break up smooth biome blobs. Higher = more varied.");
        if (ImGui::SliderFloat("Temp Lapse Rate",
                               &m_Config.temperatureLapseRate, 0.0f, 0.02f,
                               "%.4f"))
          changed = true;
        HelpMarker("Temperature decrease per block of altitude. Higher = more "
                   "dramatic snow caps on mountains.");
        if (ImGui::SliderFloat("Geothermal Gradient",
                               &m_Config.geothermalGradient, 0.0f, 0.05f,
                               "%.4f"))
          changed = true;
        HelpMarker("Temperature increase per block of depth below sea level. "
                   "Makes deep areas warmer.");

        if (changed)
          UpdatePreview();

        ImGui::EndTabItem();
      }

      // Landforms Tab
      if (ImGui::BeginTabItem("Landforms")) {
        ImGui::Dummy(ImVec2(0, 5));

        ImGui::Text("Terrain Variation Settings");
        ImGui::Separator();

        bool changed = false;

        ImGui::BeginChild("LandformScroll", ImVec2(0, 300), true);
        for (auto &[name, override] : m_Config.landformOverrides) {
          ImGui::PushID(name.c_str());
          if (ImGui::CollapsingHeader(name.c_str(),
                                      ImGuiTreeNodeFlags_DefaultOpen)) {
            float minH = 0.0f;
            float maxH = (float)m_Config.worldHeight;

            // Enforce constraints based on land/water status
            if (name == "oceans") {
              // Oceans must be below sea level
              maxH = std::max(0.0f, (float)m_Config.seaLevel - 1.0f);
            } else {
              // Land must be at or above sea level
              minH = std::min((float)m_Config.worldHeight,
                              (float)m_Config.seaLevel);
            }

            if (ImGui::SliderFloat("Base Height", &override.baseHeight, minH,
                                   maxH))
              changed = true;
            HelpMarker("Elevates this specific landform type.");
            if (ImGui::SliderFloat("Variation", &override.heightVariation, 0.0f,
                                   (float)m_Config.worldHeight / 2.0f))
              changed = true;
            HelpMarker("Amplitude of noise for this landform.");

            if (ImGui::TreeNode("Octave Amplitudes")) {
              for (int i = 0; i < 8; ++i) {
                char label[32];
                sprintf(label, "Octave %d", i);
                if (ImGui::SliderFloat(label, &override.octaveAmplitudes[i],
                                       0.0f, 1.0f))
                  changed = true;
              }
              ImGui::TreePop();
            }
          }
          ImGui::Separator();
          ImGui::PopID();
        }
        ImGui::EndChild();

        if (changed)
          UpdatePreview();

        ImGui::Separator();
        ImGui::Text("Terrain Slice Preview (X-Axis)");
        ImGui::Separator();

        // Layer visibility toggles
        ImGui::Text("Show Layers:");
        ImGui::Checkbox("Oceans", &m_ShowOceans);
        ImGui::SameLine();
        ImGui::Checkbox("Valleys", &m_ShowValleys);
        ImGui::SameLine();
        ImGui::Checkbox("Plains", &m_ShowPlains);
        ImGui::Checkbox("Hills", &m_ShowHills);
        ImGui::SameLine();
        ImGui::Checkbox("Mountains", &m_ShowMountains);
        ImGui::SameLine();
        ImGui::Checkbox("Blended", &m_ShowBlended);

        ImGui::Separator();

        // Multi-layer plot
        float availWidth = ImGui::GetContentRegionAvail().x;
        ImVec2 plotSize(availWidth, 300);
        ImVec2 plotPos = ImGui::GetCursorScreenPos();
        ImDrawList *drawList = ImGui::GetWindowDrawList();

        // Reserve space for the plot
        ImGui::InvisibleButton("##plot", plotSize);

        // Calculate plot bounds - scale to worldHeight
        float minY = 0.0f;
        float maxY = (float)m_Config.worldHeight;
        float rangeY = maxY - minY;

        // Draw background
        drawList->AddRectFilled(
            plotPos, ImVec2(plotPos.x + plotSize.x, plotPos.y + plotSize.y),
            IM_COL32(20, 20, 30, 255));

        // Draw grid lines
        for (int i = 0; i <= 4; ++i) {
          float y = plotPos.y + (plotSize.y * i / 4.0f);
          drawList->AddLine(ImVec2(plotPos.x, y),
                            ImVec2(plotPos.x + plotSize.x, y),
                            IM_COL32(60, 60, 70, 255));
        }

        // Draw sea level line
        float seaLevelY = plotPos.y + plotSize.y -
                          ((m_Config.seaLevel - minY) / rangeY * plotSize.y);
        drawList->AddLine(ImVec2(plotPos.x, seaLevelY),
                          ImVec2(plotPos.x + plotSize.x, seaLevelY),
                          IM_COL32(50, 150, 255, 200), 2.0f);

        // Helper lambda to draw a line series
        auto drawSeries = [&](float *data, ImU32 color,
                              float thickness = 1.5f) {
          for (int i = 0; i < 127; ++i) {
            float x1 = plotPos.x + (plotSize.x * i / 127.0f);
            float x2 = plotPos.x + (plotSize.x * (i + 1) / 127.0f);
            float y1 = plotPos.y + plotSize.y -
                       ((data[i] - minY) / rangeY * plotSize.y);
            float y2 = plotPos.y + plotSize.y -
                       ((data[i + 1] - minY) / rangeY * plotSize.y);
            drawList->AddLine(ImVec2(x1, y1), ImVec2(x2, y2), color, thickness);
          }
        };

        // Draw each layer if enabled
        if (m_ShowOceans)
          drawSeries(m_OceansData, IM_COL32(30, 80, 150, 180));
        if (m_ShowValleys)
          drawSeries(m_ValleysData, IM_COL32(80, 150, 120, 180));
        if (m_ShowPlains)
          drawSeries(m_PlainsData, IM_COL32(100, 180, 80, 180));
        if (m_ShowHills)
          drawSeries(m_HillsData, IM_COL32(200, 180, 60, 180));
        if (m_ShowMountains)
          drawSeries(m_MountainsData, IM_COL32(220, 100, 100, 180));

        // ALWAYS draw the blended final terrain height (white line, thicker)
        if (m_ShowBlended)
          drawSeries(m_PreviewData, IM_COL32(255, 255, 255, 255), 3.0f);

        // Draw border
        drawList->AddRect(
            plotPos, ImVec2(plotPos.x + plotSize.x, plotPos.y + plotSize.y),
            IM_COL32(100, 100, 120, 255));

        // Landform Color Bar (Cellular Selection)
        ImGui::Text("Landform Strip:");
        HelpMarker("Shows which landform is selected by cellular noise");
        ImVec2 p0 = ImGui::GetCursorScreenPos();
        float width = ImGui::GetContentRegionAvail().x;
        float height = 20.0f;
        ImVec2 p1 = ImVec2(p0.x + width, p0.y + height);

        drawList->AddRectFilled(p0, p1, IM_COL32(50, 50, 50, 255));

        float step = width / 128.0f;
        for (int i = 0; i < 128; ++i) {
          ImU32 col = IM_COL32(100, 100, 100, 255); // Default gray
          int landformId = (int)m_BiomeData[i];
          switch (landformId) {
          case 0:
            col = IM_COL32(30, 80, 150, 255);
            break; // Oceans - dark blue
          case 1:
            col = IM_COL32(80, 150, 120, 255);
            break; // Valleys - teal
          case 2:
            col = IM_COL32(100, 180, 80, 255);
            break; // Plains - green
          case 3:
            col = IM_COL32(200, 180, 60, 255);
            break; // Hills - yellow/tan
          case 4:
            col = IM_COL32(220, 100, 100, 255);
            break; // Mountains - red/brown
          }
          drawList->AddRectFilled(ImVec2(p0.x + i * step, p0.y),
                                  ImVec2(p0.x + (i + 1) * step, p1.y), col);
        }
        ImGui::Dummy(ImVec2(0, height + 5));

        ImGui::Text("Samples: 128 (X: 0 to 512)");

        ImGui::EndTabItem();
      }

      // Caves Tab
      if (ImGui::BeginTabItem("Caves")) {
        ImGui::Dummy(ImVec2(0, 5));

        ImGui::Text("Underground Generation Settings");
        ImGui::Separator();
        bool changed = false;
        if (ImGui::Checkbox("Enable Caves", &m_Config.enableCaves))
          changed = true;
        HelpMarker("Toggle generation of organic tunnel caves.");
        if (ImGui::Checkbox("Enable Ravines", &m_Config.enableRavines))
          changed = true;
        HelpMarker("Toggle generation of deep vertical cracks.");
        if (ImGui::SliderInt("Ravine Depth", &m_Config.ravineDepth, 10, 100))
          changed = true;
        HelpMarker("Maximum depth from surface for ravines.");
        if (ImGui::SliderFloat("Frequency", &m_Config.caveFrequency, 0.0f,
                               0.1f))
          changed = true;
        HelpMarker("How often cave systems attempt to spawn.");
        if (ImGui::SliderFloat("Threshold", &m_Config.caveThreshold, 0.0f,
                               1.0f))
          changed = true;
        HelpMarker("Internal noise threshold. Lower = larger caves.");
        if (ImGui::SliderFloat("Entrance Bias", &m_Config.caveEntranceNoise,
                               0.0f, 1.0f))
          changed = true;
        HelpMarker("Controls how likely caves are to break the surface.");
        if (ImGui::SliderInt("Lava Level", &m_Config.lavaLevel, 0, 40))
          changed = true;
        HelpMarker("Depth at which caves and ravines fill with lava.");
        if (ImGui::SliderFloat("Ravine Width", &m_Config.ravineWidth, 0.1f,
                               3.0f, "%.2f"))
          changed = true;
        HelpMarker("Thickness of vertical cracks.");
        if (ImGui::SliderFloat("Cave Size", &m_Config.caveSize, 0.1f, 3.0f,
                               "%.2f"))
          changed = true;
        HelpMarker("Overall scale of caverns and spaghetti tunnels.");

        if (changed)
          UpdatePreview();

        ImGui::Separator();
        ImGui::Text("Subterranean Cross-Section");
        ImGui::Separator();

        // 2D Cave Visualization
        ImDrawList *drawList = ImGui::GetWindowDrawList();
        ImVec2 plotPos = ImGui::GetCursorScreenPos();
        float availWidth = ImGui::GetContentRegionAvail().x - 10;
        ImVec2 plotSize(availWidth, 300.0f);

        // Reserve space
        ImGui::InvisibleButton("##caveslice", plotSize);

        // Draw background
        drawList->AddRectFilled(
            plotPos, ImVec2(plotPos.x + plotSize.x, plotPos.y + plotSize.y),
            IM_COL32(20, 20, 25, 255));

        float stepX = plotSize.x / 256.0f;
        float stepY = plotSize.y / 128.0f;

        // Optimisation: We'll draw the solid terrain first as columns,
        // then only draw caves/lava sections
        for (int i = 0; i < 256; i++) {
          float height = (i % 2 == 0)
                             ? m_PreviewData[i / 2]
                             : (m_PreviewData[i / 2] +
                                m_PreviewData[std::min(127, i / 2 + 1)]) *
                                   0.5f;

          float surfaceY = plotPos.y + plotSize.y -
                           (height / (float)m_Config.worldHeight) * plotSize.y;

          // Draw solid stone column up to surface
          drawList->AddRectFilled(
              ImVec2(plotPos.x + i * stepX, surfaceY),
              ImVec2(plotPos.x + (i + 1) * stepX, plotPos.y + plotSize.y),
              IM_COL32(70, 70, 75, 255));

          for (int j = 0; j < 128; j++) {
            float worldY = (float)j / 128.0f * (float)m_Config.worldHeight;

            // Only draw caves below terrain surface
            if (worldY > height)
              break;

            if (m_CaveSliceData[i + j * 256] > 0.5f) {
              // Flip Y for drawing (0 at bottom)
              float y0 = plotPos.y + plotSize.y - (j + 1) * stepY;
              float y1 = plotPos.y + plotSize.y - j * stepY;

              ImU32 col;
              if (worldY <= m_Config.lavaLevel) {
                col = IM_COL32(200, 50, 20, 255); // Lava
              } else {
                col = IM_COL32(10, 10, 15, 255); // Cave Air
              }

              drawList->AddRectFilled(ImVec2(plotPos.x + i * stepX, y0),
                                      ImVec2(plotPos.x + (i + 1) * stepX, y1),
                                      col);
            }
          }

          // Draw terrain surface line for context
          drawList->AddLine(ImVec2(plotPos.x + i * stepX, surfaceY),
                            ImVec2(plotPos.x + (i + 1) * stepX, surfaceY),
                            IM_COL32(120, 180, 80, 255), 1.0f);
        }

        // Sea Level marker
        float seaY = plotPos.y + plotSize.y -
                     ((float)m_Config.seaLevel / (float)m_Config.worldHeight) *
                         plotSize.y;
        drawList->AddLine(ImVec2(plotPos.x, seaY),
                          ImVec2(plotPos.x + plotSize.x, seaY),
                          IM_COL32(255, 255, 255, 100), 1.0f);

        // Lava Level marker
        float lavaY =
            plotPos.y + plotSize.y -
            ((float)m_Config.lavaLevel / (float)m_Config.worldHeight) *
                plotSize.y;
        drawList->AddLine(ImVec2(plotPos.x, lavaY),
                          ImVec2(plotPos.x + plotSize.x, lavaY),
                          IM_COL32(255, 100, 0, 150), 1.5f);

        ImGui::Text("High-resolution vertical slice (256x128).");

        ImGui::EndTabItem();
      }

      // Densities Tab
      if (ImGui::BeginTabItem("Densities")) {
        ImGui::Dummy(ImVec2(0, 5));
        ImGui::Text("Resource & Decorator Frequencies");
        ImGui::Separator();
        ImGui::SliderInt("Coal Attempts", &m_Config.coalAttempts, 0, 30);
        HelpMarker("Number of coal vein generation attempts per chunk.");
        ImGui::SliderInt("Iron Attempts", &m_Config.ironAttempts, 0, 20);
        HelpMarker("Number of iron vein generation attempts per chunk.");
        ImGui::SliderFloat("Oak Density", &m_Config.oakDensity, 0.0f, 20.0f);
        HelpMarker("Success rate for oak trees in forests/plains.");
        ImGui::SliderFloat("Pine Density", &m_Config.pineDensity, 0.0f, 20.0f);
        HelpMarker("Success rate for pine trees in tundra.");
        ImGui::SliderFloat("Cactus Density", &m_Config.cactusDensity, 0.0f,
                           10.0f);
        HelpMarker("Success rate for cacti in deserts.");
        ImGui::SliderFloat("Flora Density", &m_Config.floraDensity, 0.0f,
                           50.0f);
        HelpMarker("Global multiplier for grass and flowers.");
        ImGui::EndTabItem();
      }

      // Decorators Tab
      if (ImGui::BeginTabItem("Decorators")) {
        ImGui::Dummy(ImVec2(0, 5));
        ImGui::Text("Enable/Disable Features");
        ImGui::Separator();
        ImGui::Checkbox("Enable Ores", &m_Config.enableOre);
        HelpMarker("Spawn coal, iron, and other minerals.");
        ImGui::Checkbox("Enable Trees", &m_Config.enableTrees);
        HelpMarker("Spawn trees across various biomes.");
        ImGui::Checkbox("Enable Flora", &m_Config.enableFlora);
        HelpMarker("Spawn grass, flowers, and small plants.");
        ImGui::EndTabItem();
      }

      // Presets Tab
      if (ImGui::BeginTabItem("Presets")) {
        ImGui::Dummy(ImVec2(0, 5));
        ImGui::Text("Save or Load World Presets");
        ImGui::Separator();

        ImGui::InputText("Preset Name", m_ConfigName,
                         IM_ARRAYSIZE(m_ConfigName));
        HelpMarker("Name of the .json file to save or load.");

        if (ImGui::Button("Save Preset", ImVec2(120, 0))) {
          SaveConfig(m_ConfigName);
        }
        ImGui::SameLine();
        if (ImGui::Button("Load Preset", ImVec2(120, 0))) {
          LoadConfig(m_ConfigName);
        }

        ImGui::Separator();
        ImGui::Text("Presets are saved in the 'presets/' folder.");
        ImGui::EndTabItem();
      }

      // Noise Previews Tab
      if (ImGui::BeginTabItem("Noise Previews")) {
        ImGui::Dummy(ImVec2(0, 5));
        ImGui::Text("Noise Map Visualizations");
        ImGui::Separator();

        // Zoom control
        if (ImGui::SliderFloat("Preview Zoom", &m_NoisePreviewZoom, 0.5f, 3.0f,
                               "%.1fx")) {
          m_NeedsPreviewUpdate = true; // Trigger regeneration when zoom changes
        }
        HelpMarker("Zoom into noise detail (1x = 256 blocks, 2x = 128 blocks, "
                   "3x = 85 blocks)");
        ImGui::Separator();

        ImVec2 previewSize(200.0f, 200.0f); // Fixed size
        float spacing = 10.0f;

        // Row 1: Landform & Edge
        ImGui::BeginGroup();
        ImGui::Text("Landform (Cellular)");
        if (m_LandformPreview) {
          ImGui::Image((void *)(intptr_t)m_LandformPreview->GetTextureID(),
                       previewSize);
          int worldSize = static_cast<int>(256.0f / m_NoisePreviewZoom);
          ImGui::TextColored(ImVec4(0.6f, 0.8f, 0.6f, 1.0f), "~%d blocks wide",
                             worldSize);
        }
        ImGui::EndGroup();

        ImGui::SameLine(0, spacing);

        ImGui::BeginGroup();
        ImGui::Text("Edge Distance (F2-F1)");
        if (m_EdgePreview) {
          ImGui::Image((void *)(intptr_t)m_EdgePreview->GetTextureID(),
                       previewSize);
          int worldSize = static_cast<int>(256.0f / m_NoisePreviewZoom);
          ImGui::TextColored(ImVec4(0.6f, 0.8f, 0.6f, 1.0f), "~%d blocks wide",
                             worldSize);
        }
        ImGui::EndGroup();

        ImGui::SameLine(0, spacing);

        ImGui::BeginGroup();
        ImGui::Text("Terrain Detail");
        if (m_TerrainDetailPreview) {
          ImGui::Image((void *)(intptr_t)m_TerrainDetailPreview->GetTextureID(),
                       previewSize);
          int worldSize = static_cast<int>(256.0f / m_NoisePreviewZoom);
          ImGui::TextColored(ImVec4(0.6f, 0.8f, 0.6f, 1.0f), "~%d blocks wide",
                             worldSize);
        }
        ImGui::EndGroup();

        // Row 2: Temperature, Humidity, Upheaval
        ImGui::BeginGroup();
        ImGui::Text("Temperature (°C)");
        if (m_TemperaturePreview) {
          ImGui::Image((void *)(intptr_t)m_TemperaturePreview->GetTextureID(),
                       previewSize);
          int worldSize = static_cast<int>(256.0f / m_NoisePreviewZoom);
          ImGui::TextColored(ImVec4(0.6f, 0.8f, 0.6f, 1.0f), "~%d blocks wide",
                             worldSize);
        }
        ImGui::EndGroup();

        ImGui::SameLine(0, spacing);

        ImGui::BeginGroup();
        ImGui::Text("Humidity");
        if (m_HumidityPreview) {
          ImGui::Image((void *)(intptr_t)m_HumidityPreview->GetTextureID(),
                       previewSize);
          int worldSize = static_cast<int>(256.0f / m_NoisePreviewZoom);
          ImGui::TextColored(ImVec4(0.6f, 0.8f, 0.6f, 1.0f), "~%d blocks wide",
                             worldSize);
        }
        ImGui::EndGroup();

        ImGui::SameLine(0, spacing);

        ImGui::BeginGroup();
        ImGui::Text("Upheaval");
        if (m_UpheavalPreview) {
          ImGui::Image((void *)(intptr_t)m_UpheavalPreview->GetTextureID(),
                       previewSize);
          int worldSize = static_cast<int>(256.0f / m_NoisePreviewZoom);
          ImGui::TextColored(ImVec4(0.6f, 0.8f, 0.6f, 1.0f), "~%d blocks wide",
                             worldSize);
        }
        ImGui::EndGroup();

        // Row 3: Geologic
        ImGui::BeginGroup();
        ImGui::Text("Geologic Province");
        if (m_GeologicPreview) {
          ImGui::Image((void *)(intptr_t)m_GeologicPreview->GetTextureID(),
                       previewSize);
          int worldSize = static_cast<int>(256.0f / m_NoisePreviewZoom);
          ImGui::TextColored(ImVec4(0.6f, 0.8f, 0.6f, 1.0f), "~%d blocks wide",
                             worldSize);
        }
        ImGui::EndGroup();

        ImGui::Separator();

        // Noise Scale Settings
        if (ImGui::CollapsingHeader("Noise Scales",
                                    ImGuiTreeNodeFlags_DefaultOpen)) {
          bool scalesChanged = false;

          scalesChanged |=
              ImGui::SliderFloat("Landform Scale", &m_Config.landformScale,
                                 0.0001f, 0.005f, "%.4f");
          HelpMarker("Controls size of landform regions (lower = larger)");

          scalesChanged |=
              ImGui::SliderFloat("Upheaval Scale", &m_Config.upheavalScale,
                                 0.0001f, 0.005f, "%.4f");
          HelpMarker("Large-scale height variation");

          scalesChanged |= ImGui::SliderFloat("Terrain Detail Scale",
                                              &m_Config.terrainDetailScale,
                                              0.0001f, 0.01f, "%.4f");
          HelpMarker(
              "Fine detail bumps and ridges (default is landformScale*4)");

          scalesChanged |= ImGui::SliderFloat(
              "Temperature Scale", &m_Config.tempScale, 0.0005f, 0.01f, "%.4f");
          HelpMarker("Size of temperature zones");

          scalesChanged |=
              ImGui::SliderFloat("Humidity Scale", &m_Config.humidityScale,
                                 0.0005f, 0.01f, "%.4f");
          HelpMarker("Size of rainfall regions");

          scalesChanged |=
              ImGui::SliderFloat("Geologic Scale", &m_Config.geologicScale,
                                 0.0001f, 0.005f, "%.4f");
          HelpMarker("Size of rock province regions");

          scalesChanged |= ImGui::SliderFloat(
              "Forest Scale", &m_Config.forestScale, 0.01f, 0.1f, "%.3f");
          HelpMarker("Scale for tree placement noise");

          scalesChanged |= ImGui::SliderFloat("Bush Scale", &m_Config.bushScale,
                                              0.01f, 0.15f, "%.3f");
          HelpMarker("Scale for bush placement noise");

          scalesChanged |= ImGui::SliderFloat(
              "Beach Scale", &m_Config.beachScale, 0.005f, 0.05f, "%.3f");
          HelpMarker("Scale for beach detection noise");

          if (scalesChanged) {
            m_NeedsPreviewUpdate = true;
          }

          if (ImGui::Button("Reset to Defaults")) {
            WorldGenConfig defaults;
            m_Config.landformScale = defaults.landformScale;
            m_Config.upheavalScale = defaults.upheavalScale;
            m_Config.tempScale = defaults.tempScale;
            m_Config.humidityScale = defaults.humidityScale;
            m_Config.geologicScale = defaults.geologicScale;
            m_NeedsPreviewUpdate = true;
          }
        }

        if (ImGui::Button("Regenerate Previews")) {
          UpdateNoisePreviews();
        }

        ImGui::EndTabItem();
      }

      ImGui::EndTabBar();
    }
  }
  ImGui::End();

  ImGui::Render();
  ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

  if (ImGui::GetIO().ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
    GLFWwindow *backup_current_context = glfwGetCurrentContext();
    ImGui::UpdatePlatformWindows();
    ImGui::RenderPlatformWindowsDefault();
    glfwMakeContextCurrent(backup_current_context);
  }
}

void MenuState::UpdateNoisePreviews() {
  if (!m_PreviewNoiseManager)
    return;

  // Recreate NoiseManager with current config
  m_PreviewNoiseManager = std::make_unique<NoiseManager>(m_Config);

  // Calculate world size based on zoom
  // zoom 1.0 = 256 blocks, zoom 2.0 = 128 blocks (more detail), zoom 0.5 = 512
  // blocks
  int worldSize = static_cast<int>(256.0f / m_NoisePreviewZoom);
  std::vector<float> data(256 * 256); // Always 256x256 texture

  // Landform (Cellular)
  m_PreviewNoiseManager->GetPreview(NoiseManager::NoiseType::Landform,
                                    data.data(), worldSize, worldSize);
  m_LandformPreview->UpdateFromData(data.data(),
                                    NoisePreview::ColorScheme::Grayscale);

  // Edge Distance
  m_PreviewNoiseManager->GetPreview(NoiseManager::NoiseType::LandformEdge,
                                    data.data(), worldSize, worldSize);
  m_EdgePreview->UpdateFromData(data.data(),
                                NoisePreview::ColorScheme::EdgeDistance);

  // Terrain Detail
  m_PreviewNoiseManager->GetPreview(NoiseManager::NoiseType::TerrainDetail,
                                    data.data(), worldSize, worldSize);
  m_TerrainDetailPreview->UpdateFromData(data.data(),
                                         NoisePreview::ColorScheme::Terrain);

  // Temperature
  m_PreviewNoiseManager->GetPreview(NoiseManager::NoiseType::Temperature,
                                    data.data(), worldSize, worldSize);
  m_TemperaturePreview->UpdateFromData(data.data(),
                                       NoisePreview::ColorScheme::Temperature);

  // Humidity
  m_PreviewNoiseManager->GetPreview(NoiseManager::NoiseType::Humidity,
                                    data.data(), worldSize, worldSize);
  m_HumidityPreview->UpdateFromData(data.data(),
                                    NoisePreview::ColorScheme::Grayscale);

  // Upheaval
  m_PreviewNoiseManager->GetPreview(NoiseManager::NoiseType::Upheaval,
                                    data.data(), worldSize, worldSize);
  m_UpheavalPreview->UpdateFromData(data.data(),
                                    NoisePreview::ColorScheme::Terrain);

  // Geologic
  m_PreviewNoiseManager->GetPreview(NoiseManager::NoiseType::Geologic,
                                    data.data(), worldSize, worldSize);
  m_GeologicPreview->UpdateFromData(data.data(),
                                    NoisePreview::ColorScheme::Grayscale);

  m_NeedsPreviewUpdate = false;
}

void MenuState::Cleanup() {}

void MenuState::SaveConfig(const std::string &name) {
  namespace fs = std::filesystem;
  fs::path presetsDir = "presets";
  fs::path filePath = presetsDir / (name + ".json");

  try {
    fs::create_directories(presetsDir);
  } catch (const fs::filesystem_error &e) {
    LOG_ERROR("Failed to create presets directory: {}", e.what());
    return;
  }

  std::ofstream file(filePath);
  if (file.is_open()) {
    json j = m_Config;
    file << j.dump(4);
    LOG_INFO("Saved world configuration to {}", filePath.string());
  } else {
    LOG_ERROR("Failed to save configuration to {}", filePath.string());
  }
}

void MenuState::LoadConfig(const std::string &name) {
  namespace fs = std::filesystem;
  fs::path filePath = fs::path("presets") / (name + ".json");

  std::ifstream file(filePath);
  if (file.is_open()) {
    try {
      json j;
      file >> j;
      m_Config = j.get<WorldGenConfig>();
      LOG_INFO("Loaded world configuration from {}", filePath.string());
      UpdatePreview();
    } catch (const std::exception &e) {
      LOG_ERROR("JSON Load Error: {}", e.what());
    }
  } else {
    LOG_ERROR("Failed to load configuration from {}", filePath.string());
  }
}
