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
    m_PreviewData[i] = (float)tempGen.GetHeight(x, z);
    m_TempData[i] = tempGen.GetTemperature(x, z);
    m_HumidData[i] = tempGen.GetHumidity(x, z);
    m_BiomeData[i] = (int)tempGen.GetBiome(x, z);
    m_CaveProbData[i] = tempGen.GetCaveProbability(x, z);
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

        if (ImGui::SliderInt("Sea Level", &m_Config.seaLevel, 0, 128))
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

        if (changed)
          UpdatePreview();
        ImGui::EndTabItem();
      }

      // Climate Tab
      if (ImGui::BeginTabItem("Climate")) {
        ImGui::Dummy(ImVec2(0, 5));

        ImGui::Columns(2, "ClimateSplit", true);
        ImGui::SetColumnWidth(0, 300.0f);

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

        if (changed)
          UpdatePreview();

        ImGui::NextColumn();
        ImGui::Text("Climate Map (Temp vs Humid)");
        ImGui::Separator();

        // Whittaker Diagram implementation
        ImDrawList *drawList = ImGui::GetWindowDrawList();
        ImVec2 canvas_p0 = ImGui::GetCursorScreenPos();
        float canvas_sz = 220.0f;
        ImVec2 canvas_p1 =
            ImVec2(canvas_p0.x + canvas_sz, canvas_p0.y + canvas_sz);

        // Background - Draw Biome Regions
        drawList->AddRectFilled(canvas_p0, canvas_p1,
                                IM_COL32(50, 50, 50, 255));

        // Simplified biome mapping for the 2D background
        // temp: -1 to 1, humid: -1 to 1
        for (float ty = 0; ty < canvas_sz; ty += 10) {
          for (float tx = 0; tx < canvas_sz; tx += 10) {
            float t = ((tx / canvas_sz) * 2.0f) - 1.0f;
            float h = (1.0f - (ty / canvas_sz)) * 2.0f - 1.0f;

            ImU32 col = IM_COL32(100, 200, 100, 50); // Deep background
            if (t > 0.3f) {
              if (h < -0.2f)
                col = IM_COL32(200, 180, 100, 100); // Desert
              else
                col = IM_COL32(100, 200, 100, 100); // Plains
            } else if (t < -0.3f) {
              if (h > 0.2f)
                col = IM_COL32(50, 50, 120, 100); // Taiga/Podzol
              else
                col = IM_COL32(200, 200, 255, 100); // Tundra
            } else {
              if (h > 0.4f)
                col = IM_COL32(50, 100, 50, 100); // Mud/Swamp
              else
                col = IM_COL32(100, 200, 100, 100); // Plains
            }
            drawList->AddRectFilled(
                ImVec2(canvas_p0.x + tx, canvas_p0.y + ty),
                ImVec2(canvas_p0.x + tx + 10, canvas_p0.y + ty + 10), col);
          }
        }

        // Axis Labels
        drawList->AddText(ImVec2(canvas_p0.x + 5, canvas_p1.y - 15),
                          IM_COL32(255, 255, 255, 200), "Cold");
        drawList->AddText(ImVec2(canvas_p1.x - 35, canvas_p1.y - 15),
                          IM_COL32(255, 255, 255, 200), "Hot");
        drawList->AddText(ImVec2(canvas_p0.x + 5, canvas_p0.y + 5),
                          IM_COL32(255, 255, 255, 200), "Wet");
        drawList->AddText(ImVec2(canvas_p0.x + 5, canvas_p1.y - 35),
                          IM_COL32(255, 255, 255, 200), "Dry");

        // Draw the path of the world slice
        for (int i = 0; i < 127; ++i) {
          float t0 = (m_TempData[i] + 1.0f) / 2.0f;
          float h0 = (m_HumidData[i] + 1.0f) / 2.0f;
          float t1 = (m_TempData[i + 1] + 1.0f) / 2.0f;
          float h1 = (m_HumidData[i + 1] + 1.0f) / 2.0f;

          ImVec2 p0 = ImVec2(canvas_p0.x + t0 * canvas_sz,
                             canvas_p1.y - h0 * canvas_sz);
          ImVec2 p1 = ImVec2(canvas_p0.x + t1 * canvas_sz,
                             canvas_p1.y - h1 * canvas_sz);

          drawList->AddLine(p0, p1, IM_COL32(255, 255, 255, 255), 2.0f);
          if (i % 16 == 0) {
            drawList->AddCircleFilled(p0, 3.0f, IM_COL32(255, 0, 0, 255));
          }
        }

        ImGui::Dummy(ImVec2(0, canvas_sz + 10));
        ImGui::Text("Path represents 128 sampled world points.");

        ImGui::Columns(1);
        ImGui::EndTabItem();
      }

      // Landforms Tab
      if (ImGui::BeginTabItem("Landforms")) {
        ImGui::Dummy(ImVec2(0, 5));

        // Two-column layout: Parameters (Left) | Preview (Right)
        ImGui::Columns(2, "LandformSplit", true);
        ImGui::SetColumnWidth(0, 350.0f);

        ImGui::Text("Terrain Variation Settings");
        ImGui::Separator();

        bool changed = false;

        ImGui::BeginChild("LandformScroll", ImVec2(0, -20), true);
        for (auto &[name, override] : m_Config.landformOverrides) {
          ImGui::PushID(name.c_str());
          if (ImGui::CollapsingHeader(name.c_str(),
                                      ImGuiTreeNodeFlags_DefaultOpen)) {
            if (ImGui::SliderFloat("Base Height", &override.baseHeight, 0.0f,
                                   255.0f))
              changed = true;
            HelpMarker("Elevates this specific landform type.");
            if (ImGui::SliderFloat("Variation", &override.heightVariation, 0.0f,
                                   128.0f))
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

        ImGui::NextColumn();

        ImGui::Text("Terrain Slice Preview (X-Axis)");
        ImGui::Separator();

        // Render the terrain profile
        ImGui::PlotLines("##TerrainProfile", m_PreviewData, 128, 0, nullptr,
                         0.0f, 255.0f, ImVec2(0, 300));

        // Biome Color Bar
        ImGui::Text("Biome Strip:");
        ImDrawList *drawList = ImGui::GetWindowDrawList();
        ImVec2 p0 = ImGui::GetCursorScreenPos();
        float width = ImGui::GetContentRegionAvail().x;
        float height = 20.0f;
        ImVec2 p1 = ImVec2(p0.x + width, p0.y + height);

        drawList->AddRectFilled(p0, p1, IM_COL32(50, 50, 50, 255));

        float step = width / 128.0f;
        for (int i = 0; i < 128; ++i) {
          ImU32 col = IM_COL32(200, 200, 200, 255);
          switch (m_BiomeData[i]) {
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

        if (changed) {
          UpdatePreview();
        }

        ImGui::Columns(1);
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

        ImGui::NextColumn();
        ImGui::Text("Cave Probability Overlay");
        ImGui::Separator();
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
                               0.001f, 0.1f))
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

    ImGui::SetCursorPosY(ImGui::GetWindowHeight() - 60);
    ImGui::Separator();
    if (ImGui::Button("Start Game", ImVec2(-1, 40))) {
      app->ChangeState(std::make_unique<LoadingState>(m_Config));
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
