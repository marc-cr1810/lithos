#include "MenuState.h"
#include "../core/Application.h"
#include "LoadingState.h"
#include "backends/imgui_impl_glfw.h"
#include "backends/imgui_impl_opengl3.h"
#include "imgui.h"
#include <GLFW/glfw3.h>
#include <cstdio>
#include <cstring>

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
    ImGui::Text("Basic Options");
    ImGui::Separator();

    if (ImGui::InputText("Seed", m_SeedBuffer, sizeof(m_SeedBuffer))) {
      m_Config.seed = atoi(m_SeedBuffer);
    }

    ImGui::SliderInt("Sea Level", &m_Config.seaLevel, 0, 128);

    ImGui::Dummy(ImVec2(0, 10));
    ImGui::Text("Landform Options");
    ImGui::Separator();

    for (auto &[name, override] : m_Config.landformOverrides) {
      ImGui::PushID(name.c_str());
      ImGui::Text("%s", name.c_str());
      ImGui::SliderFloat("Base Height", &override.baseHeight, 0.0f, 255.0f);
      ImGui::SliderFloat("Variation", &override.heightVariation, 0.0f, 128.0f);
      ImGui::Separator();
      ImGui::PopID();
    }

    ImGui::Dummy(ImVec2(0, 10));
    ImGui::Text("Cave Options");
    ImGui::Separator();
    ImGui::SliderFloat("Frequency", &m_Config.caveFrequency, 0.0f, 0.1f);
    ImGui::SliderFloat("Threshold", &m_Config.caveThreshold, 0.0f, 1.0f);

    ImGui::Dummy(ImVec2(0, 20));
    if (ImGui::Button("Start Game", ImVec2(-1, 40))) {
      // In a real scenario, we'd pass m_Config to the application to init the
      // World. But Application currently initializes World in constructor. We
      // need to modify Application to allow World re-initialization or late
      // initialization. For now, let's just push LoadingState. Actually, we'll
      // modify Application.cpp next.
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
