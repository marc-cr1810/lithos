#include "MenuState.h"
#include "../core/Application.h"
#include "../debug/Logger.h"
#include "../world/WorldGenerator.h"
#include "LoadingState.h"
#include "backends/imgui_impl_glfw.h"
#include "backends/imgui_impl_opengl3.h"
#include "imgui.h"
#include <GLFW/glfw3.h>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>

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

void MenuState::Init(Application *app) {
  glfwSetInputMode(app->GetWindow(), GLFW_CURSOR, GLFW_CURSOR_NORMAL);

  // Randomize seed on init
  m_Config.seed = rand();
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
  WorldGenerator tempGen(m_Config);
  for (int i = 0; i < 256; ++i) {
    int x = i * 2; // More resolution, smaller step
    int z = 0;
    int height = tempGen.GetHeight(x, z);

    // Populate main preview data (128 samples for compatibility with old plots
    // if needed, but we use 256 for the new cave slice)
    if (i % 2 == 0) {
      int idx = i / 2;
      m_PreviewData[idx] = (float)height;
      m_TempData[idx] = tempGen.GetTemperature(x, z, height);
      m_HumidData[idx] = tempGen.GetHumidity(x, z);
      m_BiomeData[idx] = (float)tempGen.GetBiomeAtHeight(x, z, height);
      m_CaveProbData[idx] = tempGen.GetCaveProbability(x, z);

      // Sample individual landforms
      m_OceansData[idx] = (float)tempGen.GetHeightForLandform("oceans", x, z);
      m_ValleysData[idx] = (float)tempGen.GetHeightForLandform("valleys", x, z);
      m_PlainsData[idx] = (float)tempGen.GetHeightForLandform("plains", x, z);
      m_HillsData[idx] = (float)tempGen.GetHeightForLandform("hills", x, z);
      m_MountainsData[idx] =
          (float)tempGen.GetHeightForLandform("mountains", x, z);
    }

    // Sample 2D cave slice (X: 256, Y: 128)
    for (int j = 0; j < 128; ++j) {
      // Map j [0, 127] to height [0, worldHeight]
      int y = (int)((float)j / 128.0f * (float)m_Config.worldHeight);
      m_CaveSliceData[i + j * 256] = tempGen.IsCaveAt(x, y, 0) ? 1.0f : 0.0f;
    }
  }
}

void MenuState::HandleInput(Application *app) {}

void MenuState::Update(Application *app, float dt) {}

void MenuState::Render(Application *app) {
  int width, height;
  glfwGetFramebufferSize(app->GetWindow(), &width, &height);
  glViewport(0, 0, width, height);
  glClearColor(0.05f, 0.05f, 0.1f, 1.0f);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

void MenuState::RenderUI(Application *app) {
  ImGui_ImplOpenGL3_NewFrame();
  ImGui_ImplGlfw_NewFrame();
  ImGui::NewFrame();

  ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(),
                          ImGuiCond_Always, ImVec2(0.5f, 0.5f));
  ImGui::SetNextWindowSize(ImVec2(600, 500), ImGuiCond_FirstUseEver);

  if (ImGui::Begin("World Configuration", nullptr,
                   ImGuiWindowFlags_NoCollapse)) {
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

        if (ImGui::SliderInt("Sea Level", &m_Config.seaLevel, 0,
                             m_Config.worldHeight - 1))
          changed = true;
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

        ImGui::Text("Biome & Noise Scales");
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
        if (ImGui::SliderFloat("Landform Scale", &m_Config.landformScale,
                               0.0001f, 0.01f))
          changed = true;
        HelpMarker("Scale of landform zones (Oceans vs Mountains).");
        if (ImGui::SliderFloat("Climate Scale", &m_Config.climateScale, 0.0001f,
                               0.01f))
          changed = true;
        HelpMarker("Very large scale noise for macro-climate variations.");
        if (ImGui::SliderFloat("Geologic Scale", &m_Config.geologicScale,
                               0.0001f, 0.01f))
          changed = true;
        HelpMarker("Scale of underground strata variation.");
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

        ImGui::Separator();
        ImGui::Text("Biome Distribution Preview");
        ImGui::Separator();

        // Draw terrain cross-section with biome colors
        ImDrawList *drawList = ImGui::GetWindowDrawList();
        ImVec2 plotPos = ImGui::GetCursorScreenPos();
        float availWidth = ImGui::GetContentRegionAvail().x - 10;
        // Reduce width for the terrain plot to make room for the temp graph
        float terrainWidth = availWidth * 0.95f;
        float tempGraphWidth = availWidth * 0.025f; // Even narrower bar
        ImVec2 terrainPlotSize(terrainWidth, 200.0f);

        // Reserve space for the whole block
        ImGui::InvisibleButton("##biomeplot", ImVec2(availWidth, 200.0f));

        // --- TERRAIN PLOT ---

        // Check if mouse is hovering over the terrain plot
        if (ImGui::IsItemHovered()) {
          ImVec2 mousePos = ImGui::GetMousePos();
          // Check if within terrain bounds
          if (mousePos.x >= plotPos.x &&
              mousePos.x <= plotPos.x + terrainWidth) {
            float relX = (mousePos.x - plotPos.x) / terrainPlotSize.x;
            if (relX >= 0.0f && relX <= 1.0f) {
              int sampleIdx = (int)(relX * 128.0f);
              if (sampleIdx >= 0 && sampleIdx < 128) {
                int biome = (int)m_BiomeData[sampleIdx];
                float height = m_PreviewData[sampleIdx];

                const char *biomeName = "Unknown";
                switch (biome) {
                case 0:
                  biomeName = "Ocean";
                  break;
                case 1:
                  biomeName = "Beach";
                  break;
                case 2:
                  biomeName = "Desert";
                  break;
                case 3:
                  biomeName = "Tundra";
                  break;
                case 4:
                  biomeName = "Forest";
                  break;
                case 5:
                  biomeName = "Plains";
                  break;
                }

                ImGui::BeginTooltip();
                ImGui::Text("Biome: %s", biomeName);
                ImGui::Text("Height: %.1f", height);
                ImGui::EndTooltip();
              }
            }
          }
        }

        // Draw background for terrain
        drawList->AddRectFilled(plotPos,
                                ImVec2(plotPos.x + terrainPlotSize.x,
                                       plotPos.y + terrainPlotSize.y),
                                IM_COL32(30, 30, 35, 255));

        // Draw biome-colored terrain profile
        for (int i = 0; i < 127; i++) {
          float x0 = plotPos.x + (i / 128.0f) * terrainPlotSize.x;
          float x1 = plotPos.x + ((i + 1) / 128.0f) * terrainPlotSize.x;

          // Get height and biome for this position
          float height = m_PreviewData[i];
          float heightNext = m_PreviewData[i + 1];
          int biome = (int)m_BiomeData[i];

          // Normalize heights to plot space
          float y0 = plotPos.y + terrainPlotSize.y -
                     (height / (float)m_Config.worldHeight) * terrainPlotSize.y;
          float y1 =
              plotPos.y + terrainPlotSize.y -
              (heightNext / (float)m_Config.worldHeight) * terrainPlotSize.y;

          // Determine biome color
          ImU32 biomeColor;
          switch (biome) {
          case 0:
            biomeColor = IM_COL32(30, 60, 120, 255);
            break; // Ocean - dark blue
          case 1:
            biomeColor = IM_COL32(220, 200, 150, 255);
            break; // Beach - sandy
          case 2:
            biomeColor = IM_COL32(220, 200, 120, 255);
            break; // Desert - tan
          case 3:
            biomeColor = IM_COL32(200, 220, 240, 255);
            break; // Tundra - icy blue-white
          case 4:
            biomeColor = IM_COL32(60, 140, 60, 255);
            break; // Forest - dark green
          case 5:
            biomeColor = IM_COL32(140, 180, 100, 255);
            break; // Plains - light green
          default:
            biomeColor = IM_COL32(150, 150, 150, 255);
            break;
          }

          // Draw filled polygon from bottom to terrain height
          ImVec2 points[4] = {ImVec2(x0, plotPos.y + terrainPlotSize.y),
                              ImVec2(x0, y0), ImVec2(x1, y1),
                              ImVec2(x1, plotPos.y + terrainPlotSize.y)};
          drawList->AddConvexPolyFilled(points, 4, biomeColor);
        }

        // Draw sea level line
        float seaY = plotPos.y + terrainPlotSize.y -
                     ((float)m_Config.seaLevel / (float)m_Config.worldHeight) *
                         terrainPlotSize.y;
        drawList->AddLine(ImVec2(plotPos.x, seaY),
                          ImVec2(plotPos.x + terrainPlotSize.x, seaY),
                          IM_COL32(50, 100, 200, 200), 2.0f);

        // --- TEMPERATURE VERTICAL GRADIENT BAR ---

        ImVec2 tempPos(plotPos.x + terrainWidth + (availWidth * 0.02f),
                       plotPos.y);
        ImVec2 tempSize(tempGraphWidth, 200.0f);

        // Background/Border
        drawList->AddRectFilled(
            tempPos, ImVec2(tempPos.x + tempSize.x, tempPos.y + tempSize.y),
            IM_COL32(20, 20, 25, 255));

        // Draw gradient strips
        for (int i = 0; i < 100; ++i) {
          float relY = (float)i / 100.0f; // 0 (bottom) to 1 (top)
          int worldY = (int)(relY * m_Config.worldHeight);

          // Calculate Temp (Base - Lapse + Geothermal)
          float temp = 0.5f;
          if (worldY > m_Config.seaLevel) {
            temp -= (float)(worldY - m_Config.seaLevel) *
                    m_Config.temperatureLapseRate;
          }
          if (worldY < m_Config.seaLevel) {
            temp += (float)(m_Config.seaLevel - worldY) *
                    m_Config.geothermalGradient;
          }

          // Clamp visual range for color mapping
          // 0.0 -> Blue, 0.5 -> Green, 1.0 -> Red
          float t = std::clamp(temp, 0.0f, 1.0f);

          ImU32 col;
          // White(255,255,255) -> Green(0,255,0) -> Red(255,0,0)
          if (t < 0.5f) {
            // White to Green
            float f = t * 2.0f; // 0..1
            // R: 255->0, G: 255->255, B: 255->0
            int rb = (int)(255.0f * (1.0f - f));
            col = IM_COL32(rb, 255, rb, 255);
          } else {
            // Green to Red
            float f = (t - 0.5f) * 2.0f; // 0..1
            // R: 0->255, G: 255->0, B: 0->0
            int r = (int)(255.0f * f);
            int g = (int)(255.0f * (1.0f - f));
            col = IM_COL32(r, g, 0, 255);
          }

          float yBottom = tempPos.y + tempSize.y * (1.0f - relY);
          float yTop =
              tempPos.y + tempSize.y * (1.0f - ((float)(i + 1) / 100.0f));

          drawList->AddRectFilled(ImVec2(tempPos.x, yTop),
                                  ImVec2(tempPos.x + tempSize.x, yBottom), col);
        }

        // Sea Level Line extension
        drawList->AddLine(ImVec2(tempPos.x, seaY),
                          ImVec2(tempPos.x + tempSize.x, seaY),
                          IM_COL32(255, 255, 255, 150), 2.0f);

        // Border around bar
        drawList->AddRect(
            tempPos, ImVec2(tempPos.x + tempSize.x, tempPos.y + tempSize.y),
            IM_COL32(100, 100, 120, 255));

        // Add simple tooltip on hover
        if (ImGui::IsMouseHoveringRect(
                tempPos,
                ImVec2(tempPos.x + tempSize.x, tempPos.y + tempSize.y))) {
          ImVec2 mousePos = ImGui::GetMousePos();
          float relY = 1.0f - (mousePos.y - tempPos.y) / tempSize.y;
          int paramY = (int)(relY * m_Config.worldHeight);

          float temp = 0.5f;
          if (paramY > m_Config.seaLevel) {
            temp -= (float)(paramY - m_Config.seaLevel) *
                    m_Config.temperatureLapseRate;
          }
          if (paramY < m_Config.seaLevel) {
            temp += (float)(m_Config.seaLevel - paramY) *
                    m_Config.geothermalGradient;
          }

          ImGui::BeginTooltip();
          ImGui::Text("Height: %d", paramY);
          ImGui::Text("Base Temp: %.2f", temp);
          ImGui::EndTooltip();
        }

        // Legend
        ImGui::Text("Biome Colors:");
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(0.12f, 0.24f, 0.47f, 1.0f), "Ocean");
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(0.86f, 0.78f, 0.59f, 1.0f), "Beach");
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(0.86f, 0.78f, 0.47f, 1.0f), "Desert");
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(0.78f, 0.86f, 0.94f, 1.0f), "Tundra");
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(0.24f, 0.55f, 0.24f, 1.0f), "Forest");
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(0.55f, 0.71f, 0.39f, 1.0f), "Plains");

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
          drawSeries(m_OceansData, IM_COL32(30, 80, 150, 255));
        if (m_ShowValleys)
          drawSeries(m_ValleysData, IM_COL32(80, 150, 120, 255));
        if (m_ShowPlains)
          drawSeries(m_PlainsData, IM_COL32(100, 180, 80, 255));
        if (m_ShowHills)
          drawSeries(m_HillsData, IM_COL32(200, 180, 60, 255));
        if (m_ShowMountains)
          drawSeries(m_MountainsData, IM_COL32(220, 100, 100, 255));
        if (m_ShowBlended)
          drawSeries(m_PreviewData, IM_COL32(255, 255, 255, 255), 2.5f);

        // Draw border
        drawList->AddRect(
            plotPos, ImVec2(plotPos.x + plotSize.x, plotPos.y + plotSize.y),
            IM_COL32(100, 100, 120, 255));

        // Biome Color Bar
        ImGui::Text("Biome Strip:");
        ImVec2 p0 = ImGui::GetCursorScreenPos();
        float width = ImGui::GetContentRegionAvail().x;
        float height = 20.0f;
        ImVec2 p1 = ImVec2(p0.x + width, p0.y + height);

        drawList->AddRectFilled(p0, p1, IM_COL32(50, 50, 50, 255));

        float step = width / 128.0f;
        for (int i = 0; i < 128; ++i) {
          ImU32 col = IM_COL32(200, 200, 200, 255);
          switch ((int)m_BiomeData[i]) {
          case 0:
            col = IM_COL32(30, 60, 180, 255);
            break; // Ocean
          case 1:
            col = IM_COL32(200, 180, 100, 255);
            break; // Desert
          case 2:
            col = IM_COL32(100, 200, 100, 255);
            break; // Plains
          case 3:
            col = IM_COL32(50, 150, 50, 255);
            break; // Forest
          case 4:
            col = IM_COL32(100, 100, 150, 255);
            break; // Hills
          case 5:
            col = IM_COL32(200, 200, 200, 255);
            break; // Mountains
          case 6:
            col = IM_COL32(220, 220, 255, 255);
            break; // Tundra
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

      // Hydrology Tab
      if (ImGui::BeginTabItem("Hydrology")) {
        ImGui::Dummy(ImVec2(0, 5));
        ImGui::Text("Rivers & Lakes Configuration");
        ImGui::Separator();
        bool changed = false;
        if (ImGui::Checkbox("Enable Rivers", &m_Config.enableRivers))
          changed = true;
        HelpMarker("Toggle generation of narrow river channels.");
        if (ImGui::SliderFloat("River Scale", &m_Config.riverScale, 0.0001f,
                               0.02f))
          changed = true;
        HelpMarker("Controls the frequency/wiggle of rivers.");
        if (ImGui::SliderFloat("River Threshold", &m_Config.riverThreshold,
                               0.001f, 0.2f))
          changed = true;
        HelpMarker("Controls the width of the river channels.");
        if (ImGui::SliderFloat("River Depth", &m_Config.riverDepth, 1.0f,
                               32.0f))
          changed = true;
        HelpMarker("Maximum depth of river channel cuts.");
        HelpMarker("Depth of water at the bottom of river channels.");
        if (ImGui::SliderInt("Lake Level", &m_Config.lakeLevel, 0, 100))
          changed = true;
        HelpMarker("Basins below this level will fill with water.");

        if (changed)
          UpdatePreview();
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
      UpdatePreview();
    } catch (const std::exception &e) {
      LOG_ERROR("JSON Load Error: {}", e.what());
    }
  } else {
    LOG_ERROR("Failed to load configuration from {}", filePath.string());
  }
}
