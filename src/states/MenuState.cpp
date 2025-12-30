#include "MenuState.h"
#include "../core/Application.h"
#include "../world/WorldGenerator.h"
#include "LoadingState.h"
#include "backends/imgui_impl_glfw.h"
#include "backends/imgui_impl_opengl3.h"
#include "imgui.h"
#include <GLFW/glfw3.h>
#include <cstdio>
#include <cstring>

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
  initLandform("plains", 66.0f, 15.0f,
               {0.55f, 0.28f, 0.14f, 0.07f, 0.035f, 0.018f, 0.009f, 0.0045f,
                0.0022f, 0.0011f});
  initLandform("hills", 72.0f, 40.0f,
               {0.45f, 0.38f, 0.28f, 0.2f, 0.12f, 0.07f, 0.035f, 0.018f, 0.009f,
                0.0045f});
  initLandform(
      "mountains", 85.0f, 120.0f,
      {0.38f, 0.45f, 0.5f, 0.42f, 0.28f, 0.2f, 0.14f, 0.07f, 0.035f, 0.018f});
  initLandform("valleys", 55.0f, 20.0f,
               {0.65f, 0.22f, 0.11f, 0.055f, 0.028f, 0.014f, 0.007f, 0.0035f,
                0.017f, 0.0008f});

  UpdatePreview();
}

void MenuState::UpdatePreview() {
  WorldGenerator tempGen(m_Config);
  for (int i = 0; i < 128; ++i) {
    int x = i * 4;
    int z = 0;
    int height = tempGen.GetHeight(x, z);
    m_PreviewData[i] = (float)height;
    m_TempData[i] = tempGen.GetTemperature(x, z);
    m_HumidData[i] = tempGen.GetHumidity(x, z);
    m_BiomeData[i] =
        (float)tempGen.GetBiomeAtHeight(x, z, height); // Use height-aware biome
    m_CaveProbData[i] = tempGen.GetCaveProbability(x, z);

    // Sample individual landforms
    m_OceansData[i] = (float)tempGen.GetHeightForLandform("oceans", x, z);
    m_ValleysData[i] = (float)tempGen.GetHeightForLandform("valleys", x, z);
    m_PlainsData[i] = (float)tempGen.GetHeightForLandform("plains", x, z);
    m_HillsData[i] = (float)tempGen.GetHeightForLandform("hills", x, z);
    m_MountainsData[i] = (float)tempGen.GetHeightForLandform("mountains", x, z);
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

        if (changed)
          UpdatePreview();

        ImGui::Separator();
        ImGui::Text("Biome Distribution Preview");
        ImGui::Separator();

        // Draw terrain cross-section with biome colors
        ImDrawList *drawList = ImGui::GetWindowDrawList();
        ImVec2 plotPos = ImGui::GetCursorScreenPos();
        ImVec2 plotSize(ImGui::GetContentRegionAvail().x - 10, 200.0f);

        // Reserve space
        ImGui::InvisibleButton("##biomeplot", plotSize);

        // Check if mouse is hovering over the plot
        if (ImGui::IsItemHovered()) {
          ImVec2 mousePos = ImGui::GetMousePos();
          float relX = (mousePos.x - plotPos.x) / plotSize.x;
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

        // Draw background
        drawList->AddRectFilled(
            plotPos, ImVec2(plotPos.x + plotSize.x, plotPos.y + plotSize.y),
            IM_COL32(30, 30, 35, 255));

        // Draw biome-colored terrain profile
        for (int i = 0; i < 127; i++) {
          float x0 = plotPos.x + (i / 128.0f) * plotSize.x;
          float x1 = plotPos.x + ((i + 1) / 128.0f) * plotSize.x;

          // Get height and biome for this position
          float height = m_PreviewData[i];
          float heightNext = m_PreviewData[i + 1];
          int biome = (int)m_BiomeData[i];

          // Normalize heights to plot space
          float y0 = plotPos.y + plotSize.y -
                     (height / (float)m_Config.worldHeight) * plotSize.y;
          float y1 = plotPos.y + plotSize.y -
                     (heightNext / (float)m_Config.worldHeight) * plotSize.y;

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
          ImVec2 points[4] = {ImVec2(x0, plotPos.y + plotSize.y),
                              ImVec2(x0, y0), ImVec2(x1, y1),
                              ImVec2(x1, plotPos.y + plotSize.y)};
          drawList->AddConvexPolyFilled(points, 4, biomeColor);
        }

        // Draw sea level line
        float seaY = plotPos.y + plotSize.y -
                     ((float)m_Config.seaLevel / (float)m_Config.worldHeight) *
                         plotSize.y;
        drawList->AddLine(ImVec2(plotPos.x, seaY),
                          ImVec2(plotPos.x + plotSize.x, seaY),
                          IM_COL32(50, 100, 200, 200), 2.0f);

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
            if (ImGui::SliderFloat("Base Height", &override.baseHeight, 0.0f,
                                   (float)m_Config.worldHeight))
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

        ImGui::Columns(2, "CaveSplit", true);
        ImGui::SetColumnWidth(0, 300.0f);

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

        if (changed)
          UpdatePreview();

        ImGui::Separator();
        ImGui::Text("Terrain Profile Visualization");
        ImGui::Separator();

        // Terrain visualization
        ImGui::PlotLines("Probability", m_CaveProbData, 128, 0, nullptr, 0.0f,
                         1.0f, ImVec2(0, 200));
        ImGui::Text("Showing avg cave presence 0-64Y");

        ImGui::Columns(1);
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
