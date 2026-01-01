#include "Application.h"
#include "../debug/Logger.h"
#include "../debug/Profiler.h"
#include "../states/LoadingState.h"
#include "../states/MenuState.h"
#include "StateManager.h"

// ImGui
#include "backends/imgui_impl_glfw.h"
#include "backends/imgui_impl_opengl3.h"
#include "imgui.h"

#include "../world/Block.h"
#include <iostream>


void framebuffer_size_callback(GLFWwindow *window, int width, int height) {
  auto app = reinterpret_cast<Application *>(glfwGetWindowUserPointer(window));
  if (app)
    app->OnResize(width, height);
}

Application::Application(const AppConfig &config)
    : m_Config(config), m_Camera(glm::vec3(0.0f, 20.0f, 3.0f)) {
  Init();

  // Init Application Dependent Systems
  m_StateManager = std::make_unique<StateManager>(this);
  // m_World will be initialized in LoadingState::Init

  // Load initial state
  m_StateManager->PushState(std::make_unique<MenuState>());
}

Application::~Application() { Shutdown(); }

void Application::Init() {
  // Window Init (Moved from main.cpp)
  glfwInit();
  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
  glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
#ifdef __APPLE__
  glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif

  m_Window = glfwCreateWindow(m_Config.width, m_Config.height,
                              m_Config.title.c_str(), NULL, NULL);
  if (m_Window == NULL) {
    LOG_ERROR("Failed to create GLFW window");
    glfwTerminate();
    exit(-1);
  }
  glfwMakeContextCurrent(m_Window);
  glfwSwapInterval(m_Config.vsync ? 1 : 0);

  glfwSetWindowUserPointer(m_Window, this);
  glfwSetFramebufferSizeCallback(m_Window, framebuffer_size_callback);

#ifdef USE_GLEW
  glewExperimental = GL_TRUE;
  if (glewInit() != GLEW_OK) {
    LOG_ERROR("Failed to initialize GLEW");
    exit(-1);
  }
#endif

  glEnable(GL_DEPTH_TEST);
  glEnable(GL_CULL_FACE);
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  glCullFace(GL_BACK);

  InitImGui();

  // Load Global Resources
  // Basic Shader
  m_ResourceManager.LoadShader("basic", "src/shaders/basic.vs",
                               "src/shaders/basic.fs");
  // Texture Atlas
  m_ResourceManager.LoadTextureAtlas("blocks", "assets/textures/block");
  // Resolve UVs for blocks globally once
  if (auto *atlas = m_ResourceManager.GetTextureAtlas("blocks")) {
    BlockRegistry::getInstance().resolveUVs(*atlas);
  }

  // Apply Config
  m_Camera.Zoom = m_Config.fov;
}

void Application::InitImGui() {
  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  ImGuiIO &io = ImGui::GetIO();
  (void)io;
  io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
  io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
  io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;

  ImGui::StyleColorsDark();
  ImGuiStyle &style = ImGui::GetStyle();
  if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
    style.WindowRounding = 0.0f;
    style.Colors[ImGuiCol_WindowBg].w = 1.0f;
  }

  ImGui_ImplGlfw_InitForOpenGL(m_Window, true);
  ImGui_ImplOpenGL3_Init("#version 130");
}

void Application::Run() {
  float lastFrame = 0.0f;

  while (!glfwWindowShouldClose(m_Window) && m_Running) {
    PROFILE_SCOPE("Main Loop");
    float currentFrame = static_cast<float>(glfwGetTime());
    float deltaTime = currentFrame - lastFrame;
    lastFrame = currentFrame;

    // Poll Events
    glfwPollEvents();

    // Process State Changes
    m_StateManager->ProcessStateChange();

    // Update
    m_StateManager->Update(deltaTime);

    // Render
    // int width, height;
    // glfwGetFramebufferSize(m_Window, &width, &height);

    m_StateManager->Render();

    // Swap Buffers
    if (glfwGetWindowAttrib(m_Window, GLFW_VISIBLE))
      glfwSwapBuffers(m_Window);
  }
}

void Application::Quit() { m_Running = false; }

void Application::OnResize(int width, int height) {
  glViewport(0, 0, width, height);
}

void Application::PushState(std::unique_ptr<State> state) {
  m_StateManager->PushState(std::move(state));
}

void Application::PopState() { m_StateManager->PopState(); }

void Application::ChangeState(std::unique_ptr<State> state) {
  m_StateManager->ChangeState(std::move(state));
}

void Application::Shutdown() {
  ImGui_ImplOpenGL3_Shutdown();
  ImGui_ImplGlfw_Shutdown();
  ImGui::DestroyContext();
  glfwTerminate();
}
