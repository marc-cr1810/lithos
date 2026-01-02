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
#include <random>


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

  // No initial world update - only generate on Play or Benchmark
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

  // World generation deferred to Play or Benchmark
}

// UpdatePreview was removed as 1D/2D previews are legacy.
// World generation is now explicitly triggered by Play or Benchmark.

void MenuState::HandleInput(Application *app) {}

void MenuState::Update(Application *app, float dt) {}

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
      std::random_device rd;
      std::mt19937 gen(rd());
      m_Config.seed = (int)gen();
      snprintf(m_SeedBuffer, sizeof(m_SeedBuffer), "%d", m_Config.seed);
      changed = true;
    }
    if (changed) {
      // Seed updated, but we don't auto-generate world here anymore.
      // Generation will happen when clicking Play or Run Benchmark.
    }

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
          std::random_device rd;
          std::mt19937 gen(rd());
          m_Config.seed = (int)gen();
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
        if (m_Config.worldHeight > 512) {
          ImGui::TextColored(
              ImVec4(1.0f, 0.6f, 0.0f, 1.0f),
              "WARNING: Heights above 512 may impact performance!");
        }

        if (changed) {
          // General settings updated, but we don't auto-generate world here.
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
      // No auto-generate here either.
    } catch (const std::exception &e) {
      LOG_ERROR("JSON Load Error: {}", e.what());
    }
  } else {
    LOG_ERROR("Failed to load configuration from {}", filePath.string());
  }
}
