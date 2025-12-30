#include "MenuState.h"
#include "../core/Application.h"
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

        if (ImGui::InputText("Seed", m_SeedBuffer, sizeof(m_SeedBuffer))) {
          m_Config.seed = atoi(m_SeedBuffer);
        }
        HelpMarker(
            "The seed used for noise generation. Same seed = same world.");
        ImGui::SameLine();
        if (ImGui::Button("Randomize")) {
          m_Config.seed = rand();
          snprintf(m_SeedBuffer, sizeof(m_SeedBuffer), "%d", m_Config.seed);
        }

        ImGui::SliderInt("Sea Level", &m_Config.seaLevel, 0, 128);
        HelpMarker("The Y-level where water fills up to.");
        ImGui::SliderFloat("Terrain Scale", &m_Config.terrainScale, 0.0001f,
                           0.01f);
        HelpMarker("Controls the 'zoom' level of the terrain noise. Lower = "
                   "larger features.");
        ImGui::SliderInt("Surface Depth", &m_Config.surfaceDepth, 1, 10);
        HelpMarker(
            "How many blocks of 'surface' material (dirt/sand) are above "
            "the stone strata.");
        ImGui::SliderFloat("Global Scale", &m_Config.globalScale, 0.1f, 5.0f);
        HelpMarker("Multiplier for all terrain heights.");
        ImGui::EndTabItem();
      }

      // Climate Tab
      if (ImGui::BeginTabItem("Climate")) {
        ImGui::Dummy(ImVec2(0, 5));
        ImGui::Text("Biome & Noise Scales");
        ImGui::Separator();
        ImGui::SliderFloat("Temp Scale", &m_Config.tempScale, 0.0001f, 0.01f);
        HelpMarker("Scale of temperature variation. Controls biome size.");
        ImGui::SliderFloat("Humidity Scale", &m_Config.humidityScale, 0.0001f,
                           0.01f);
        HelpMarker("Scale of rainfall variation. Affects vegetation.");
        ImGui::SliderFloat("Landform Scale", &m_Config.landformScale, 0.0001f,
                           0.01f);
        HelpMarker("Scale of landform zones (Oceans vs Mountains).");
        ImGui::SliderFloat("Climate Scale", &m_Config.climateScale, 0.0001f,
                           0.01f);
        HelpMarker("Very large scale noise for macro-climate variations.");
        ImGui::SliderFloat("Geologic Scale", &m_Config.geologicScale, 0.0001f,
                           0.01f);
        HelpMarker("Scale of underground strata variation.");
        ImGui::EndTabItem();
      }

      // Landforms Tab
      if (ImGui::BeginTabItem("Landforms")) {
        ImGui::Dummy(ImVec2(0, 5));
        ImGui::Text("Terrain Variation Settings");
        ImGui::Separator();

        ImGui::BeginChild("LandformScroll", ImVec2(0, -60), true);
        for (auto &[name, override] : m_Config.landformOverrides) {
          ImGui::PushID(name.c_str());
          ImGui::Text("%s", name.c_str());
          ImGui::SliderFloat("Base Height", &override.baseHeight, 0.0f, 255.0f);
          HelpMarker("Elevates this specific landform type.");
          ImGui::SliderFloat("Variation", &override.heightVariation, 0.0f,
                             128.0f);
          HelpMarker("Amplitude of noise for this landform.");
          ImGui::Separator();
          ImGui::PopID();
        }
        ImGui::EndChild();
        ImGui::EndTabItem();
      }

      // Caves Tab
      if (ImGui::BeginTabItem("Caves")) {
        ImGui::Dummy(ImVec2(0, 5));
        ImGui::Text("Underground Generation Settings");
        ImGui::Separator();
        ImGui::Checkbox("Enable Caves", &m_Config.enableCaves);
        HelpMarker("Toggle generation of organic tunnel caves.");
        ImGui::Checkbox("Enable Ravines", &m_Config.enableRavines);
        HelpMarker("Toggle generation of deep vertical cracks.");
        ImGui::SliderInt("Ravine Depth", &m_Config.ravineDepth, 10, 100);
        HelpMarker("Maximum depth from surface for ravines.");
        ImGui::SliderFloat("Frequency", &m_Config.caveFrequency, 0.0f, 0.1f);
        HelpMarker("How often cave systems attempt to spawn.");
        ImGui::SliderFloat("Threshold", &m_Config.caveThreshold, 0.0f, 1.0f);
        HelpMarker("Internal noise threshold. Lower = larger caves.");
        ImGui::SliderFloat("Entrance Bias", &m_Config.caveEntranceNoise, 0.0f,
                           1.0f);
        HelpMarker("Controls how likely caves are to break the surface.");
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
