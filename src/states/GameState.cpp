#include "GameState.h"
#include "../core/Application.h"
#include "../debug/Logger.h"
#include "../debug/Profiler.h"
#include "../ecs/Components.h"
#include "../ecs/Systems.h"
#include "../world/Block.h"

#include "backends/imgui_impl_glfw.h"
#include "backends/imgui_impl_opengl3.h"
#include "imgui.h"

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

// Helper
// Helper
static std::string BlockIdToName(int type) {
  return BlockRegistry::getInstance().getBlock(type)->getName();
}

GameState::GameState(glm::vec3 spawnPos) : m_SpawnPos(spawnPos) {}

void GameState::Init(Application *app) {
  LOG_INFO("Entering Game State");
  glfwSetInputMode(app->GetWindow(), GLFW_CURSOR, GLFW_CURSOR_DISABLED);

  int width, height;
  glfwGetFramebufferSize(app->GetWindow(), &width, &height);
  m_LastX = width / 2.0f;
  m_LastY = height / 2.0f;

  app->GetCamera().Position = m_SpawnPos + glm::vec3(0.0f, 1.6f, 0.0f);

  InitEntities(app);
  InitRendering();

  m_DbgTeleportPos[0] = m_SpawnPos.x;
  m_DbgTeleportPos[1] = m_SpawnPos.y;
  m_DbgTeleportPos[2] = m_SpawnPos.z;

  m_DbgRenderDistance = app->GetConfig().renderDistance;
}

void GameState::InitEntities(Application *app) {
  auto &registry = app->GetRegistry();
  auto &camera = app->GetCamera();

  m_PlayerEntity = registry.create();
  registry.emplace<TransformComponent>(m_PlayerEntity, m_SpawnPos,
                                       glm::vec3(0.0f), glm::vec3(1.0f));
  registry.emplace<VelocityComponent>(m_PlayerEntity, glm::vec3(0.0f));
  registry.emplace<GravityComponent>(m_PlayerEntity, 45.0f);
  registry.emplace<CameraComponent>(m_PlayerEntity, camera.Front, camera.Right,
                                    camera.Up, camera.WorldUp, camera.Yaw,
                                    camera.Pitch, camera.Zoom);
  registry.emplace<InputComponent>(m_PlayerEntity, 0.1f, 6.0f, 10.5f, false,
                                   false, false, false);
  registry.emplace<PlayerTag>(m_PlayerEntity);
}

void GameState::InitRendering() {
  auto &rm = ResourceManager::Get();
  m_Shader = rm.GetShader("basic");
  m_Atlas = rm.GetTextureAtlas("blocks");
  m_BlockTexture = rm.GetTexture("blocks");

  // Note: UVs are resolved in Application::Init globally.

  if (m_Shader) {
    m_Shader->use();
    m_Shader->setInt("texture1", 0);
    if (m_Atlas) {
      float uScale = 16.0f / m_Atlas->GetWidth();
      float vScale = 16.0f / m_Atlas->GetHeight();
      m_Shader->setVec2("uvScale", uScale, vScale);
    }
  }

  // Crosshair
  float crosshairVertices[] = {
      -0.025f, 0.0f,    0.0f, 1.0f, 1.0f, 1.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f,
      0.025f,  0.0f,    0.0f, 1.0f, 1.0f, 1.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f,
      0.0f,    -0.025f, 0.0f, 1.0f, 1.0f, 1.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f,
      0.0f,    0.025f,  0.0f, 1.0f, 1.0f, 1.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f};
  glGenVertexArrays(1, &m_CrosshairVAO);
  glGenBuffers(1, &m_CrosshairVBO);
  glBindVertexArray(m_CrosshairVAO);
  glBindBuffer(GL_ARRAY_BUFFER, m_CrosshairVBO);
  glBufferData(GL_ARRAY_BUFFER, sizeof(crosshairVertices), crosshairVertices,
               GL_STATIC_DRAW);
  glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 11 * sizeof(float),
                        (void *)0);
  glEnableVertexAttribArray(0);
  glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 11 * sizeof(float),
                        (void *)(3 * sizeof(float)));
  glEnableVertexAttribArray(1);
  glVertexAttribPointer(3, 3, GL_FLOAT, GL_FALSE, 11 * sizeof(float),
                        (void *)(8 * sizeof(float)));
  glEnableVertexAttribArray(3);

  // SelectBox
  glGenVertexArrays(1, &m_SelectVAO);
  glGenBuffers(1, &m_SelectVBO);
  glBindVertexArray(m_SelectVAO);
  glBindBuffer(GL_ARRAY_BUFFER, m_SelectVBO);
  glBufferData(GL_ARRAY_BUFFER, sizeof(float) * 24 * 11, NULL, GL_DYNAMIC_DRAW);
  glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 11 * sizeof(float),
                        (void *)0);
  glEnableVertexAttribArray(0);
  glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 11 * sizeof(float),
                        (void *)(3 * sizeof(float)));
  glEnableVertexAttribArray(1);
  glVertexAttribPointer(3, 3, GL_FLOAT, GL_FALSE, 11 * sizeof(float),
                        (void *)(8 * sizeof(float)));
  glEnableVertexAttribArray(3);
}

void GameState::HandleInput(Application *app) {
  GLFWwindow *window = app->GetWindow();

  // Escape to Pause
  static bool lastEscState = false;
  bool currentEsc = glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS;
  if (currentEsc && !lastEscState) {
    m_IsPaused = !m_IsPaused;
    if (m_IsPaused) {
      glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
    } else {
      glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
      m_FirstMouse = true;
    }
  }
  lastEscState = currentEsc;

  if (m_IsPaused)
    return;

  // Debug Toggles (M = Mouse/Debug Mode, P = Profiler)
  static bool lastMState = false;
  bool currentM = glfwGetKey(window, GLFW_KEY_M) == GLFW_PRESS;
  if (currentM && !lastMState) {
    m_IsDebugMode = !m_IsDebugMode;
    if (m_IsDebugMode) {
      glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
    } else {
      glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
      m_FirstMouse = true;
    }
  }
  lastMState = currentM;

  static bool lastPState = false;
  bool currentP = glfwGetKey(window, GLFW_KEY_P) == GLFW_PRESS;
  if (currentP && !lastPState) {
    m_ShowProfiler = !m_ShowProfiler;
  }
  lastPState = currentP;

  // ECS Input Handling
  auto &input = app->GetRegistry().get<InputComponent>(m_PlayerEntity);

  // Sprint Logic
  static bool lastWState = false;
  static float lastWTime = -1.0f;
  static bool lastCtrlState = false;

  bool currentWState = glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS;
  bool currentSState = glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS;
  bool currentAState = glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS;
  bool currentDState = glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS;
  bool ctrlPressed = glfwGetKey(window, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS;
  bool isMoving =
      currentWState || currentSState || currentAState || currentDState;

  // Toggle Sprint with Ctrl
  if (ctrlPressed && !lastCtrlState) {
    input.isSprinting = !input.isSprinting;
  }
  lastCtrlState = ctrlPressed;

  // Double Tap W to Enable Sprint
  if (currentWState && !lastWState) {
    float currentTime = (float)glfwGetTime();
    if (currentTime - lastWTime < 0.3f) {
      input.isSprinting = true;
    }
    lastWTime = currentTime;
  }
  lastWState = currentWState;

  // Reset Sprint if stopped moving
  if (!isMoving) {
    input.isSprinting = false;
  }

  // Mouse Polling & Camera Update
  if (!m_IsDebugMode && !m_IsPaused) {
    double xpos, ypos;
    glfwGetCursorPos(window, &xpos, &ypos);

    if (m_FirstMouse) {
      m_LastX = (float)xpos;
      m_LastY = (float)ypos;
      m_FirstMouse = false;
    }

    float xoffset = (float)xpos - m_LastX;
    float yoffset =
        m_LastY -
        (float)ypos; // reversed since y-coordinates go from bottom to top

    m_LastX = (float)xpos;
    m_LastY = (float)ypos;

    // Update Camera Component
    auto &camComp = app->GetRegistry().get<CameraComponent>(m_PlayerEntity);
    float sensitivity = input.mouseSensitivity;
    xoffset *= sensitivity;
    yoffset *= sensitivity;

    camComp.yaw += xoffset;
    camComp.pitch += yoffset;

    // Constrain pitch
    if (camComp.pitch > 89.0f)
      camComp.pitch = 89.0f;
    if (camComp.pitch < -89.0f)
      camComp.pitch = -89.0f;
  }

  // Interaction (Destroy/Place) - Logic from old_main.cpp main loop
  if (m_Hit && !m_IsDebugMode) { // Only if not in debug/cursor mode
    bool mouseCaptured = ImGui::GetIO().WantCaptureMouse;
    if (!input.noclip) {
      // Destruction (Left Click)
      static bool lastLeftMouse = false;
      bool currentLeftMouse =
          glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS;
      if (currentLeftMouse && !lastLeftMouse && !mouseCaptured) {
        app->GetWorld()->setBlock(m_HitPos.x, m_HitPos.y, m_HitPos.z, AIR);
      }
      lastLeftMouse = currentLeftMouse;

      // Placement (Right Click)
      static bool lastRightMouse = false;
      bool currentRightMouse =
          glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS;
      if (currentRightMouse && !lastRightMouse && !mouseCaptured) {
        // Prevent placing block inside player (AABB check)
        float playerWidth = 0.6f;
        float playerHeight = 1.8f;
        glm::vec3 pMin =
            app->GetCamera().Position - glm::vec3(0.0f, 1.6f, 0.0f) -
            glm::vec3(playerWidth / 2.0f, 0.0f, playerWidth / 2.0f);
        glm::vec3 pMax =
            pMin + glm::vec3(playerWidth, playerHeight, playerWidth);

        glm::vec3 bMin((float)m_PrePos.x, (float)m_PrePos.y, (float)m_PrePos.z);
        glm::vec3 bMax = bMin + glm::vec3(1.0f);

        bool collision = (pMin.x <= bMax.x && pMax.x >= bMin.x) &&
                         (pMin.y <= bMax.y && pMax.y >= bMin.y) &&
                         (pMin.z <= bMax.z && pMax.z >= bMin.z);

        if (!collision || !BlockRegistry::getInstance()
                               .getBlock(m_SelectedBlock)
                               ->isSolid()) {
          app->GetWorld()->setBlock(m_PrePos.x, m_PrePos.y, m_PrePos.z,
                                    m_SelectedBlock);
          if (m_SelectedBlockMetadata > 0) {
            app->GetWorld()->setMetadata(m_PrePos.x, m_PrePos.y, m_PrePos.z,
                                         m_SelectedBlockMetadata);
          }
        }
      }
      lastRightMouse = currentRightMouse;
    }
  }

  // Hotbar Keys
  if (glfwGetKey(window, GLFW_KEY_1) == GLFW_PRESS) {
    m_SelectedBlock = DIRT;
    m_SelectedBlockMetadata = 0;
  }
  if (glfwGetKey(window, GLFW_KEY_2) == GLFW_PRESS) {
    m_SelectedBlock = STONE;
    m_SelectedBlockMetadata = 0;
  }
  if (glfwGetKey(window, GLFW_KEY_3) == GLFW_PRESS) {
    m_SelectedBlock = GRASS;
    m_SelectedBlockMetadata = 0;
  }
  if (glfwGetKey(window, GLFW_KEY_4) == GLFW_PRESS) {
    m_SelectedBlock = WOOD;
    m_SelectedBlockMetadata = 0;
  }
  if (glfwGetKey(window, GLFW_KEY_5) == GLFW_PRESS) {
    m_SelectedBlock = WOOD_PLANKS;
    m_SelectedBlockMetadata = 0;
  }
  if (glfwGetKey(window, GLFW_KEY_6) == GLFW_PRESS) {
    m_SelectedBlock = COBBLESTONE;
    m_SelectedBlockMetadata = 0;
  }
  if (glfwGetKey(window, GLFW_KEY_7) == GLFW_PRESS) {
    m_SelectedBlock = OBSIDIAN;
    m_SelectedBlockMetadata = 0;
  }
  if (glfwGetKey(window, GLFW_KEY_8) == GLFW_PRESS) {
    m_SelectedBlock = SAND;
    m_SelectedBlockMetadata = 0;
  }
  if (glfwGetKey(window, GLFW_KEY_9) == GLFW_PRESS) {
    m_SelectedBlock = GLOWSTONE;
    m_SelectedBlockMetadata = 0;
  }
  if (glfwGetKey(window, GLFW_KEY_0) == GLFW_PRESS) {
    m_SelectedBlock = WATER;
    m_SelectedBlockMetadata = 0;
  }
}

void GameState::Update(Application *app, float dt) {
  // Clamp delta time to 0.1f (from old_main.cpp)
  dt = std::min(dt, 0.1f);

  if (!m_DbgTimePaused && !m_IsPaused) {
    m_GlobalTime += dt * m_DbgTimeSpeed;
    // Update Animations
    if (m_Atlas->Update(dt * m_DbgTimeSpeed)) {
      m_Atlas->UpdateTextureGPU(m_BlockTexture->ID);
    }
  }

  // Physics Loop
  if (!m_IsPaused) {
    m_TickAccumulator += dt;
    const float TICK_RATE = 20.0f;
    const float TICK_INTERVAL = 1.0f / TICK_RATE;

    {
      PROFILE_SCOPE("Physics Tick");
      while (m_TickAccumulator >= TICK_INTERVAL) {
        app->GetWorld()->Tick();
        m_TickAccumulator -= TICK_INTERVAL;
      }
    }
    {
      PROFILE_SCOPE("World Update");
      app->GetWorld()->Update();
    }

    // Player Control / Physics
    GLFWwindow *window = app->GetWindow();
    bool currentW = glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS;
    bool currentS = glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS;
    bool currentA = glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS;
    bool currentD = glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS;

    auto &input = app->GetRegistry().get<InputComponent>(m_PlayerEntity);
    bool up = false;
    bool down = false;
    if (input.flyMode || input.noclip) {
      if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS)
        up = true;
      if (glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS)
        down = true;
    } else {
      if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS)
        up = true;
    }

    {
      PROFILE_SCOPE("ECS Update");
      PhysicsSystem::Update(app->GetRegistry(), dt);
      PlayerControlSystem::Update(app->GetRegistry(), currentW, currentS,
                                  currentA, currentD, up, down, dt,
                                  *app->GetWorld());
      CameraSystem::Update(app->GetRegistry(), app->GetCamera());
    }
  }

  // LOD Check
  static float lodTimer = 0.0f;
  lodTimer += dt;
  if (lodTimer > 0.5f) {
    lodTimer = 0.0f;
    int width, height;
    glfwGetFramebufferSize(app->GetWindow(), &width, &height);
    // Avoid division by zero
    if (height == 0)
      height = 1;

    glm::mat4 projection =
        glm::perspective(glm::radians(app->GetCamera().Zoom),
                         (float)width / (float)height, 0.1f, 1000.0f);
    glm::mat4 view = app->GetCamera().GetViewMatrix();

    {
      PROFILE_SCOPE("Chunk Manager");
      auto &tx = app->GetRegistry().get<TransformComponent>(m_PlayerEntity);
      app->GetWorld()->loadChunks(tx.position, m_DbgRenderDistance,
                                  projection * view);
      app->GetWorld()->unloadChunks(tx.position, m_DbgRenderDistance);
    }
  }

  // Sun Strength
  const float cycleFactor = 3.14159265f / 1200.0f;
  m_SunStrength = (sin(m_GlobalTime * cycleFactor) + 1.0f) * 0.5f;
  m_SunStrength = std::max(0.05f, m_SunStrength);

  // Raycast
  {
    PROFILE_SCOPE("Raycast");
    m_Hit = app->GetWorld()->raycast(app->GetCamera().Position,
                                     app->GetCamera().Front, 5.0f, m_HitPos,
                                     m_PrePos);
  }
}

void GameState::Render(Application *app) {
  auto &input = app->GetRegistry().get<InputComponent>(m_PlayerEntity);
  int width, height;
  glfwGetFramebufferSize(app->GetWindow(), &width, &height);
  // Water/Lava Tinting Logic
  bool inWater = false;
  // Get camera block
  ChunkBlock camBlock =
      app->GetWorld()->getBlock((int)floor(app->GetCamera().Position.x),
                                (int)floor(app->GetCamera().Position.y),
                                (int)floor(app->GetCamera().Position.z));
  int camBlockType = camBlock.getType();
  if (camBlockType == WATER || camBlockType == LAVA)
    inWater = true;

  float dayFactor = m_SunStrength;
  if (dayFactor < 0.2f)
    dayFactor = 0.2f;

  glm::vec3 skyColor = glm::vec3(0.5f, 0.7f, 1.0f) * dayFactor;
  if (dayFactor < 0.2f)
    skyColor = glm::vec3(0.1f, 0.14f, 0.2f); // Night fallback

  glm::vec3 fogCol = glm::vec3(0.5f, 0.6f, 0.7f) * dayFactor;
  float fDist = m_DbgFogDist;
  bool uFog = m_DbgUseFog;

  if (inWater) {
    // Underwater Blue
    if (camBlockType == WATER) {
      skyColor = glm::vec3(0.1f, 0.1f, 0.4f) * m_SunStrength;
      fogCol = glm::vec3(0.05f, 0.05f, 0.3f) * m_SunStrength;
      fDist = 15.0f;
    } else {
      // Lava Red
      skyColor = glm::vec3(0.6f, 0.1f, 0.0f);
      fogCol = glm::vec3(0.5f, 0.0f, 0.0f);
      fDist = 5.0f;
    }
    uFog = true; // Force fog
  }

  glClearColor(skyColor.x, skyColor.y, skyColor.z, 1.0f);
  glEnable(GL_DEPTH_TEST);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

  m_Shader->use();

  glm::mat4 projection =
      glm::perspective(glm::radians(app->GetCamera().Zoom),
                       (float)width / (float)height, 0.1f, 1000.0f);
  glm::mat4 view = app->GetCamera().GetViewMatrix();

  m_Shader->setMat4("projection", projection);
  m_Shader->setMat4("view", view);
  m_Shader->setBool("useLighting", true);
  m_Shader->setVec3("viewPos", app->GetCamera().Position);
  m_Shader->setVec3("lightColor", 1.0f, 1.0f, 1.0f);
  m_Shader->setVec3("lightDir", 0.0f, 1.0f, 0.2f);
  m_Shader->setFloat("sunStrength", m_SunStrength);
  m_Shader->setBool("useHeatmap", m_DbgUseHeatmap);
  m_Shader->setBool("useFog", uFog);
  m_Shader->setFloat("fogDist", fDist);
  m_Shader->setVec3("fogColor", fogCol);

  // Enable textures for world rendering
  m_Shader->setBool("useTexture", true);

  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, m_BlockTexture->ID);

  if (m_DbgWireframe)
    glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);

  glm::mat4 cullMatrix = projection * view;
  if (m_DbgFreezeCulling) {
    cullMatrix = m_FrozenViewProj;
  } else {
    m_FrozenViewProj = cullMatrix;
  }

  // Basic Render
  {
    PROFILE_SCOPE("Render Chunks");
    app->GetWorld()->render(*m_Shader, cullMatrix, app->GetCamera().Position,
                            m_DbgRenderDistance);
  }

  if (m_DbgWireframe)
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

  if (m_DbgChunkBorders) {
    app->GetWorld()->renderDebugBorders(*m_Shader, projection * view);
  }

  // Crosshair
  float aspect = (float)width / (float)height;
  glm::mat4 crosshairModel =
      glm::scale(glm::mat4(1.0f), glm::vec3(1.0f / aspect, 1.0f, 1.0f));
  m_Shader->setMat4("model", crosshairModel);
  m_Shader->setMat4("view", glm::mat4(1.0f));
  m_Shader->setMat4("projection", glm::mat4(1.0f));
  m_Shader->setBool("useTexture", false);
  m_Shader->setBool("useLighting", false);
  m_Shader->setBool("useFog", false);

  glBindVertexArray(m_CrosshairVAO);
  glDrawArrays(GL_LINES, 0, 4);

  // Block Selection Box
  // Block Selection Box
  if (m_Hit && !input.noclip) {
    // Draw Selection Wireframe (World Space Vertices like old_main.cpp)
    float gap = 0.001f;
    float minX = m_HitPos.x - gap;
    float maxX = m_HitPos.x + 1 + gap;
    float minY = m_HitPos.y - gap;
    float maxY = m_HitPos.y + 1 + gap;
    float minZ = m_HitPos.z - gap;
    float maxZ = m_HitPos.z + 1 + gap;
    // White lines
    float r = 1.0f, g = 1.0f, b = 1.0f;

    // 11 floats per vertex: x,y,z, r,g,b, u,v, nx,ny,nz
    float boxVerts[] = {minX, minY, minZ, r, g, b, 0, 0, 1, 1, 1,
                        maxX, minY, minZ, r, g, b, 0, 0, 1, 1, 1,
                        maxX, minY, minZ, r, g, b, 0, 0, 1, 1, 1,
                        maxX, maxY, minZ, r, g, b, 0, 0, 1, 1, 1,
                        maxX, maxY, minZ, r, g, b, 0, 0, 1, 1, 1,
                        minX, maxY, minZ, r, g, b, 0, 0, 1, 1, 1,
                        minX, maxY, minZ, r, g, b, 0, 0, 1, 1, 1,
                        minX, minY, minZ, r, g, b, 0, 0, 1, 1, 1,

                        minX, minY, maxZ, r, g, b, 0, 0, 1, 1, 1,
                        maxX, minY, maxZ, r, g, b, 0, 0, 1, 1, 1,
                        maxX, minY, maxZ, r, g, b, 0, 0, 1, 1, 1,
                        maxX, maxY, maxZ, r, g, b, 0, 0, 1, 1, 1,
                        maxX, maxY, maxZ, r, g, b, 0, 0, 1, 1, 1,
                        minX, maxY, maxZ, r, g, b, 0, 0, 1, 1, 1,
                        minX, maxY, maxZ, r, g, b, 0, 0, 1, 1, 1,
                        minX, minY, maxZ, r, g, b, 0, 0, 1, 1, 1,

                        minX, minY, minZ, r, g, b, 0, 0, 1, 1, 1,
                        minX, minY, maxZ, r, g, b, 0, 0, 1, 1, 1,
                        maxX, minY, minZ, r, g, b, 0, 0, 1, 1, 1,
                        maxX, minY, maxZ, r, g, b, 0, 0, 1, 1, 1,
                        maxX, maxY, minZ, r, g, b, 0, 0, 1, 1, 1,
                        maxX, maxY, maxZ, r, g, b, 0, 0, 1, 1, 1,
                        minX, maxY, minZ, r, g, b, 0, 0, 1, 1, 1,
                        minX, maxY, maxZ, r, g, b, 0, 0, 1, 1, 1};

    glBindVertexArray(m_SelectVAO);
    glBindBuffer(GL_ARRAY_BUFFER, m_SelectVBO);
    glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(boxVerts), boxVerts);

    m_Shader->setMat4("model", glm::mat4(1.0f));
    m_Shader->setMat4("view", view);
    m_Shader->setMat4("projection", projection);
    m_Shader->setBool("useTexture", false);
    m_Shader->setBool("useLighting", false);
    m_Shader->setBool("useFog", false);

    glDrawArrays(GL_LINES, 0, 24);
  }
}

void GameState::RenderUI(Application *app) {
  GLFWwindow *window = app->GetWindow();
  ImGui_ImplOpenGL3_NewFrame();
  ImGui_ImplGlfw_NewFrame();
  ImGui::NewFrame();

  if (m_IsPaused) {
    ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(),
                            ImGuiCond_Always, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(300, 200));

    if (ImGui::Begin("Pause Menu", nullptr,
                     ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                         ImGuiWindowFlags_NoResize)) {
      float windowWidth = ImGui::GetWindowSize().x;
      ImGui::Dummy(ImVec2(0.0f, 20.0f));
      std::string text = "GAME PAUSED";
      float textWidth = ImGui::CalcTextSize(text.c_str()).x;
      ImGui::SetCursorPosX((windowWidth - textWidth) * 0.5f);
      ImGui::Text("%s", text.c_str());

      ImGui::Dummy(ImVec2(0.0f, 30.0f));

      float buttonWidth = 200.0f;
      ImGui::SetCursorPosX((windowWidth - buttonWidth) * 0.5f);
      if (ImGui::Button("Resume", ImVec2(buttonWidth, 40.0f))) {
        m_IsPaused = false;
        glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
        m_FirstMouse = true;
      }

      ImGui::Dummy(ImVec2(0.0f, 10.0f));
      ImGui::SetCursorPosX((windowWidth - buttonWidth) * 0.5f);
      if (ImGui::Button("Quit to Desktop", ImVec2(buttonWidth, 40.0f))) {
        app->Quit();
      }
    }
    ImGui::End();
  }

  if (m_IsDebugMode) {
    ImGui::Begin("Debug Info");

    auto &transform =
        app->GetRegistry().get<TransformComponent>(m_PlayerEntity);
    auto &velocity = app->GetRegistry().get<VelocityComponent>(m_PlayerEntity);
    auto &camComp = app->GetRegistry().get<CameraComponent>(m_PlayerEntity);
    auto &input = app->GetRegistry().get<InputComponent>(m_PlayerEntity);
    auto &gravity = app->GetRegistry().get<GravityComponent>(m_PlayerEntity);

    if (ImGui::CollapsingHeader("Stats", ImGuiTreeNodeFlags_DefaultOpen)) {
      ImGui::Text("FPS: %.1f (%.3f ms)", ImGui::GetIO().Framerate,
                  1000.0f / ImGui::GetIO().Framerate);

      m_DbgFrametimes[m_DbgFrametimeOffset] =
          1000.0f / ImGui::GetIO().Framerate; // Approx
      m_DbgFrametimeOffset = (m_DbgFrametimeOffset + 1) % 120;

      ImGui::PlotLines("Frame Time", m_DbgFrametimes, 120, m_DbgFrametimeOffset,
                       "ms", 0.0f, 50.0f, ImVec2(0, 80));

      ImGui::Separator();
      ImGui::Text("Position: %.2f, %.2f, %.2f", transform.position.x,
                  transform.position.y, transform.position.z);
      ImGui::Text("Velocity: %.2f, %.2f, %.2f", velocity.velocity.x,
                  velocity.velocity.y, velocity.velocity.z);
      ImGui::Text("Yaw: %.1f, Pitch: %.1f", camComp.yaw, camComp.pitch);
      ImGui::Text("Grounded: %s", input.isGrounded ? "Yes" : "No");

      if (ImGui::Button("Teleport")) {
        transform.position = glm::vec3(m_DbgTeleportPos[0], m_DbgTeleportPos[1],
                                       m_DbgTeleportPos[2]);
        velocity.velocity = glm::vec3(0.0f);
      }
      ImGui::SameLine();
      ImGui::InputFloat3("##pos", m_DbgTeleportPos);
    }

    // FOV
    if (ImGui::SliderFloat("FOV", &camComp.zoom, 1.0f, 120.0f)) {
      app->GetCamera().Zoom = camComp.zoom;
    }

    // VSync
    if (ImGui::Checkbox("VSync", &m_DbgVsync)) {
      glfwSwapInterval(m_DbgVsync ? 1 : 0);
    }

    ImGui::Separator();
    ImGui::Text("Time Controls");
    if (ImGui::Button(m_DbgTimePaused ? "Resume" : "Pause")) {
      m_DbgTimePaused = !m_DbgTimePaused;
    }
    ImGui::SameLine();
    ImGui::SliderFloat("Speed", &m_DbgTimeSpeed, 0.0f, 10.0f);
    ImGui::SliderFloat("Time", &m_GlobalTime, 0.0f, 2400.0f);

    ImGui::Separator();
    ImGui::Text("Player / Render");
    int currentMode = 0;
    if (input.flyMode && !input.noclip)
      currentMode = 1;
    if (input.noclip)
      currentMode = 2;

    ImGui::Text("Game Mode:");
    if (ImGui::RadioButton("Normal", currentMode == 0)) {
      input.flyMode = false;
      input.noclip = false;
    }
    ImGui::SameLine();
    if (ImGui::RadioButton("Creative", currentMode == 1)) {
      input.flyMode = true;
      input.noclip = false;
    }
    ImGui::SameLine();
    if (ImGui::RadioButton("Spectator", currentMode == 2)) {
      input.flyMode = false;
      input.noclip = true;
    }

    ImGui::Checkbox("Wireframe", &m_DbgWireframe);
    // Config render distance
    if (ImGui::SliderInt("Render Dist", &m_DbgRenderDistance, 2, 32)) {
      glm::mat4 proj = glm::perspective(glm::radians(app->GetCamera().Zoom),
                                        (float)app->GetConfig().width /
                                            (float)app->GetConfig().height,
                                        0.1f, 1000.0f);
      app->GetWorld()->loadChunks(transform.position, m_DbgRenderDistance,
                                  proj * app->GetCamera().GetViewMatrix());
    }
    ImGui::SliderInt("Simulation Dist", &m_DbgSimulationDistance, 1, 16);
    ImGui::Text("Chunks Loaded: %zu", app->GetWorld()->getChunkCount());
    ImGui::SliderFloat("Gravity", &gravity.strength, 0.0f, 50.0f);
    ImGui::SameLine();
    ImGui::Checkbox("Freeze Culling", &m_DbgFreezeCulling);

    ImGui::Separator();
    ImGui::Text("Visualization");
    ImGui::Checkbox("Chunk Borders", &m_DbgChunkBorders);
    ImGui::Checkbox("Light Heatmap", &m_DbgUseHeatmap);
    ImGui::Checkbox("Fog", &m_DbgUseFog);
    if (m_DbgUseFog) {
      ImGui::SliderFloat("Fog Dist", &m_DbgFogDist, 10.0f, 200.0f);
    }

    if (ImGui::CollapsingHeader("Creative Menu",
                                ImGuiTreeNodeFlags_DefaultOpen)) {
      int buttonsPerRow = 5;
      // All blocks except AIR (0)
      int blocks[] = {DIRT,           GRASS,       STONE,
                      WOOD,           LEAVES,      COAL_ORE,
                      IRON_ORE,       GLOWSTONE,   WATER,
                      LAVA,           SAND,        GRAVEL,
                      SNOW,           ICE,         CACTUS,
                      PINE_WOOD,      PINE_LEAVES, TALL_GRASS,
                      DEAD_BUSH,      ROSE,        DRY_SHORT_GRASS,
                      DRY_TALL_GRASS, OBSIDIAN,    COBBLESTONE,
                      WOOD_PLANKS};
      int numBlocks = sizeof(blocks) / sizeof(blocks[0]);

      for (int i = 0; i < numBlocks; ++i) {
        if (i > 0 && i % buttonsPerRow != 0)
          ImGui::SameLine();
        std::string label = BlockIdToName(blocks[i]);
        if (ImGui::Button((label + "##btn").c_str(), ImVec2(60, 60))) {
          m_SelectedBlock = (BlockType)blocks[i];
          m_SelectedBlockMetadata = 0;
        }
        if ((int)m_SelectedBlock == blocks[i] && m_SelectedBlockMetadata == 0) {
          ImGui::GetWindowDrawList()->AddRect(ImGui::GetItemRectMin(),
                                              ImGui::GetItemRectMax(),
                                              IM_COL32(255, 255, 0, 255), 3.0f);
        }
      }
      if (numBlocks % buttonsPerRow != 0)
        ImGui::SameLine();
      if (ImGui::Button("Spruce Planks##btn", ImVec2(60, 60))) {
        m_SelectedBlock = WOOD_PLANKS;
        m_SelectedBlockMetadata = 1;
      }
      if (m_SelectedBlock == WOOD_PLANKS && m_SelectedBlockMetadata == 1) {
        ImGui::GetWindowDrawList()->AddRect(ImGui::GetItemRectMin(),
                                            ImGui::GetItemRectMax(),
                                            IM_COL32(255, 255, 0, 255), 3.0f);
      }
      ImGui::Text("Selected: %s (Meta: %d)", BlockIdToName(m_SelectedBlock),
                  m_SelectedBlockMetadata);
    }

    if (ImGui::CollapsingHeader("Raycast", ImGuiTreeNodeFlags_DefaultOpen)) {
      if (m_Hit) {
        ChunkBlock cb =
            app->GetWorld()->getBlock(m_HitPos.x, m_HitPos.y, m_HitPos.z);
        ImGui::Text("Hit Block: %s (%d)", BlockIdToName(cb.getType()),
                    cb.getType());
        ImGui::Text("Hit Pos: %d, %d, %d", m_HitPos.x, m_HitPos.y, m_HitPos.z);
        ImGui::Text("Pre Pos: %d, %d, %d", m_PrePos.x, m_PrePos.y, m_PrePos.z);
      } else {
        ImGui::Text("No Hit");
      }
    }

    ImGui::Separator();
#ifdef LITHOS_DEBUG
    ImGui::TextColored(ImVec4(1, 1, 0, 1), "DEBUG BUILD");
#else
    ImGui::TextColored(ImVec4(0, 1, 0, 1), "RELEASE BUILD");
#endif
    ImGui::End();
  }

  // Profiler
  if (m_IsDebugMode || m_ShowProfiler) {
    if (!m_IsDebugMode) {
      ImGui::SetNextWindowBgAlpha(0.35f);
      ImGui::SetNextWindowPos(ImVec2(10, 10), ImGuiCond_FirstUseEver);
      ImGui::SetNextWindowSize(ImVec2(300, 400), ImGuiCond_FirstUseEver);
    }
    ImGuiWindowFlags flags = 0;
    if (!m_IsDebugMode) {
      flags |= ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoInputs |
               ImGuiWindowFlags_AlwaysAutoResize |
               ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav;
    }
    if (ImGui::Begin("Profiler", nullptr, flags)) {
      if (!m_IsDebugMode) {
        ImGui::Text("Profiler Overlay (Press P to toggle, M for Mouse)");
        ImGui::Separator();
      }
      auto &results = Profiler::Get().GetResults();
      for (auto &[name, times] : results) {
        if (!times.empty()) {
          char label[50];
          sprintf(label, "%.3fms", times.back());
          ImGui::PlotLines(name.c_str(), times.data(), (int)times.size(), 0,
                           label, 0.0f, 20.0f, ImVec2(0, 50));
        }
      }
    }
    ImGui::End();
  }

  ImGui::Render();
  ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
  if (ImGui::GetIO().ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
    GLFWwindow *backup_current_context = glfwGetCurrentContext();
    ImGui::UpdatePlatformWindows();
    ImGui::RenderPlatformWindowsDefault();
    glfwMakeContextCurrent(backup_current_context);
  }
}

void GameState::Cleanup() {}
