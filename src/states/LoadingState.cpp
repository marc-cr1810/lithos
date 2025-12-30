#include "LoadingState.h"
#include "../core/Application.h"
#include "../debug/Logger.h"
#include "GameState.h"

#include "backends/imgui_impl_glfw.h"
#include "backends/imgui_impl_opengl3.h"
#include "imgui.h"
#include <GLFW/glfw3.h>

LoadingState::LoadingState(const WorldGenConfig &config) : m_Config(config) {}

void LoadingState::Init(Application *app) {
  LOG_INFO("Entering Loading State");

  // Re-initialize World with our new config
  app->SetWorld(std::make_unique<World>(m_Config));

  glfwSetInputMode(app->GetWindow(), GLFW_CURSOR, GLFW_CURSOR_NORMAL);

  m_LoadingStartTime = glfwGetTime();

#ifdef LITHOS_DEBUG
  m_SpawnRadius = 2;
#else
  m_SpawnRadius = 8;
#endif

  LOG_WORLD_INFO("Generating Spawn Area... (Radius {} Chunks)", m_SpawnRadius);
}

void LoadingState::HandleInput(Application *app) {
  // Process minimal input if needed (e.g. force quit)
  if (glfwGetKey(app->GetWindow(), GLFW_KEY_ESCAPE) == GLFW_PRESS)
    app->Quit();
}

void LoadingState::Update(Application *app, float dt) {
  double currentTime = glfwGetTime();

  // Timeout Check
  if (currentTime - m_LoadingStartTime > 60.0) {
    LOG_WORLD_WARN("Spawn Ground NOT Found (Timeout). Using Air Drop.");
    m_FoundGround = true; // Force proceed
  }

  // Drive World Generation
  static double lastLoadTime = 0;
  if (currentTime - lastLoadTime > 0.1) {
    // Dummy values for View/Proj during loading, or use camera if we want to
    // visualize it
    glm::mat4 view = app->GetCamera().GetViewMatrix();
    glm::mat4 projection = glm::perspective(glm::radians(app->GetCamera().Zoom),
                                            (float)app->GetConfig().width /
                                                (float)app->GetConfig().height,
                                            0.1f, 1000.0f);

    app->GetWorld()->loadChunks(glm::vec3(m_SpawnX, 100, m_SpawnZ),
                                m_SpawnRadius, projection * view);
    lastLoadTime = currentTime;
  }
  app->GetWorld()->Update();

  // Check Progress
  int cx = (m_SpawnX >= 0) ? (m_SpawnX / CHUNK_SIZE)
                           : ((m_SpawnX + 1) / CHUNK_SIZE - 1);
  int cz = (m_SpawnZ >= 0) ? (m_SpawnZ / CHUNK_SIZE)
                           : ((m_SpawnZ + 1) / CHUNK_SIZE - 1);

  m_LoadedCount = 0;
  m_TotalChunksToCheck = 0;
  bool allLoaded = true;

  for (int rx = cx - m_SpawnRadius; rx <= cx + m_SpawnRadius; ++rx) {
    for (int rz = cz - m_SpawnRadius; rz <= cz + m_SpawnRadius; ++rz) {
      int dx = rx - cx;
      int dz = rz - cz;
      if (dx * dx + dz * dz <= m_SpawnRadius * m_SpawnRadius) {
        m_TotalChunksToCheck++;
        if (app->GetWorld()->getChunk(rx, 4, rz) != nullptr) {
          m_LoadedCount++;
        } else {
          allLoaded = false;
        }
      }
    }
  }

  if (allLoaded) {
    // Search for ground
    for (int y = 255; y > 0; --y) {
      if (app->GetWorld()->getChunk(cx, y / CHUNK_SIZE, cz) != nullptr) {
        ChunkBlock b = app->GetWorld()->getBlock(m_SpawnX, y, m_SpawnZ);
        if (b.isActive()) {
          m_SpawnY = (float)y + 2.5f;
          m_FoundGround = true;
          LOG_WORLD_INFO("Spawn Ground Found at Y={}", y);
          break;
        }
      }
    }

    // If loaded but no ground found (void world?), just proceed
    if (!m_FoundGround) {
      LOG_WORLD_WARN("No ground found, spawning in air.");
      m_FoundGround = true;
    }
  }

  if (m_FoundGround) {
    // Transition to GameState
    // Calculate Spawn Pos
    glm::vec3 spawnPos((float)m_SpawnX + 0.5f, m_SpawnY,
                       (float)m_SpawnZ + 0.5f);

    // Switch State
    // Note: ChangeState queues the change, so it happens next frame
    // We pass the spawn position to the GameState
    // (Wait, GameState constructor or Init needs it. Let's make GameState
    // accept it in constructor) But ChangeState takes uniq_ptr<State>, so we
    // construct it here.

    // We might want to pass data. A simple struct or Constructor args work.
    app->ChangeState(std::make_unique<GameState>(spawnPos));
  }
}

void LoadingState::Render(Application *app) {
  int width, height;
  glfwGetFramebufferSize(app->GetWindow(), &width, &height);
  glViewport(0, 0, width, height);
  glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

void LoadingState::RenderUI(Application *app) {
  ImGui_ImplOpenGL3_NewFrame();
  ImGui_ImplGlfw_NewFrame();
  ImGui::NewFrame();

  // ImGui Loading Screen Logic (Copied from main.cpp)
  int cx = (m_SpawnX >= 0) ? (m_SpawnX / CHUNK_SIZE)
                           : ((m_SpawnX + 1) / CHUNK_SIZE - 1);
  int cz = (m_SpawnZ >= 0) ? (m_SpawnZ / CHUNK_SIZE)
                           : ((m_SpawnZ + 1) / CHUNK_SIZE - 1);

  const int gridSize = (m_SpawnRadius * 2 + 1);
  const float cellSize = 12.0f;
  const float gridWidth = gridSize * cellSize;
  const float gridHeight = gridSize * cellSize;
  const float windowWidth = std::max(500.0f, gridWidth + 40.0f);
  const float windowHeight = 200.0f + gridHeight;

  ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(),
                          ImGuiCond_Always, ImVec2(0.5f, 0.5f));
  ImGui::SetNextWindowSize(ImVec2(windowWidth, windowHeight));

  if (ImGui::Begin("Loading", nullptr,
                   ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                       ImGuiWindowFlags_NoSavedSettings)) {
    ImGui::Dummy(ImVec2(0.0f, 10.0f));
    float contentWidth = ImGui::GetWindowSize().x;
    float textWidth = ImGui::CalcTextSize("Generating World...").x;
    ImGui::SetCursorPosX((contentWidth - textWidth) * 0.5f);
    ImGui::Text("Generating World...");

    ImGui::Dummy(ImVec2(0.0f, 10.0f));
    ImGui::Separator();
    ImGui::Dummy(ImVec2(0.0f, 10.0f));

    ImGui::Text("Loading Chunks: %d / %d", m_LoadedCount, m_TotalChunksToCheck);
    float progress = (m_TotalChunksToCheck > 0)
                         ? ((float)m_LoadedCount / m_TotalChunksToCheck)
                         : 0.0f;
    ImGui::ProgressBar(progress, ImVec2(-1.0f, 20.0f));

    ImGui::Dummy(ImVec2(0.0f, 15.0f));
    ImGui::Text("Chunk Loading Status:");
    ImGui::Dummy(ImVec2(0.0f, 5.0f));

    ImGui::SetCursorPosX((contentWidth - gridWidth) * 0.5f);
    ImVec2 gridStart = ImGui::GetCursorScreenPos();
    ImDrawList *drawList = ImGui::GetWindowDrawList();

    for (int rx = cx - m_SpawnRadius; rx <= cx + m_SpawnRadius; ++rx) {
      for (int rz = cz - m_SpawnRadius; rz <= cz + m_SpawnRadius; ++rz) {
        int dx = rx - cx;
        int dz = rz - cz;
        int gridX = dx + m_SpawnRadius;
        int gridZ = m_SpawnRadius - dz;

        ImVec2 cellMin(gridStart.x + gridX * cellSize,
                       gridStart.y + gridZ * cellSize);
        ImVec2 cellMax(cellMin.x + cellSize - 2, cellMin.y + cellSize - 2);

        ImU32 color;
        if (dx * dx + dz * dz <= m_SpawnRadius * m_SpawnRadius) {
          if (app->GetWorld()->getChunk(rx, 4, rz) != nullptr) {
            color = IM_COL32(50, 200, 50, 255);
          } else {
            color = IM_COL32(100, 100, 100, 255);
          }
        } else {
          color = IM_COL32(40, 40, 40, 255);
        }
        drawList->AddRectFilled(cellMin, cellMax, color);
        if (rx == cx && rz == cz) {
          drawList->AddRect(cellMin, cellMax, IM_COL32(255, 255, 0, 255), 0.0f,
                            0, 2.0f);
        }
      }
    }
    ImGui::Dummy(ImVec2(gridWidth, gridHeight));
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

void LoadingState::Cleanup() {
  // Nothing specific to cleanup, World persists
}
