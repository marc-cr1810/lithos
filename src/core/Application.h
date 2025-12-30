#pragma once

#include <memory>
#include <string>

#ifdef USE_GLEW
#include <GL/glew.h>
#endif
#include <GLFW/glfw3.h>

#include "../render/Camera.h"
#include "../world/World.h"
#include "StateManager.h"
#include <entt/entt.hpp>

struct AppConfig {
  int width = 1280;
  int height = 720;
  std::string title = "Lithos";
  int seed = 0;
  bool vsync = false;
  int renderDistance = 8;
  float fov = 45.0f;
};

class Application {
public:
  Application(const AppConfig &config);
  ~Application();

  void Run();
  void Quit();

  // State Management
  void PushState(std::unique_ptr<State> state);
  void PopState();
  void ChangeState(std::unique_ptr<State> state);

  // Getters for States
  GLFWwindow *GetWindow() const { return m_Window; }
  World *GetWorld() { return m_World.get(); }
  void SetWorld(std::unique_ptr<World> world) { m_World = std::move(world); }
  entt::registry &GetRegistry() { return m_Registry; }
  Camera &GetCamera() { return m_Camera; }

  const AppConfig &GetConfig() const { return m_Config; }

  // Callbacks
  void OnResize(int width, int height);

private:
  void Init();
  void InitImGui();
  void Shutdown();

  AppConfig m_Config;
  GLFWwindow *m_Window = nullptr;

  std::unique_ptr<StateManager> m_StateManager;

  // Game Systems that are persistent
  std::unique_ptr<World> m_World;
  entt::registry m_Registry;
  Camera m_Camera;

  bool m_Running = true;
};
