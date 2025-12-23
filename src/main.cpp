#include <iostream>
#include <vector>
#include <thread>
#include <atomic>
#include <chrono>
#include <algorithm>

#ifdef USE_GLEW
#include <GL/glew.h>
#endif

#include <GLFW/glfw3.h>
#include "imgui.h"
#include "backends/imgui_impl_glfw.h"
#include "backends/imgui_impl_opengl3.h"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "render/Shader.h"
#include "render/Camera.h"
#include "render/Texture.h"
#include "render/TextureAtlas.h"
#include "world/World.h"
#include "world/Player.h"
#include "world/WorldGenerator.h"


void framebuffer_size_callback(GLFWwindow* window, int width, int height);
void processInput(GLFWwindow *window, const World& world); 

// settings
// settings
unsigned int SCR_WIDTH = 1280;
unsigned int SCR_HEIGHT = 720;

// camera &// camera
Camera camera(glm::vec3(0.0f, 20.0f, 3.0f));
// Player (Position will be reset in main)
Player player(glm::vec3(0.0f, 20.0f, 3.0f));
float lastX = SCR_WIDTH / 2.0f;
float lastY = SCR_HEIGHT / 2.0f;
bool firstMouse = true;

// timing
float deltaTime = 0.0f;	// time between current frame and last frame
float lastFrame = 0.0f;

// Debug State
bool isDebugMode = false;
bool lastM = false;

// Debug UI State
float dbg_teleport_pos[3] = {0.0f, 0.0f, 0.0f};
float dbg_frametimes[120] = {0};
int dbg_frametime_offset = 0;
bool dbg_vsync = false; // Default off or detect?
bool dbg_timePaused = false;
float dbg_timeSpeed = 1.0f;
bool dbg_wireframe = false;
int dbg_renderDistance = 8;
bool dbg_chunkBorders = false;
bool dbg_useHeatmap = false;
bool dbg_useFog = false;
float dbg_fogDist = 50.0f;
bool dbg_freezeCulling = false;
glm::mat4 dbg_frozenProjView(1.0f);
int dbg_renderedChunks = 0;

// Helper
const char* GetBlockName(int type) {
    switch(type) {
        case 0: return "AIR";
        case 1: return "DIRT";
        case 2: return "GRASS";
        case 3: return "STONE";
        case 4: return "WOOD";
        case 5: return "LEAVES";
        case COAL_ORE: return "Coal Ore";
        case IRON_ORE: return "Iron Ore";
        case GLOWSTONE: return "Glowstone";
        case WATER: return "Water";
        case LAVA: return "Lava";
        case SAND: return "Sand";
        case GRAVEL: return "Gravel";
        default: return "Unknown";
    }
}

int main()
{
    // glfw: initialize and configure
    // ------------------------------
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif

    // glfw window creation
    // --------------------
    GLFWwindow* window = glfwCreateWindow(SCR_WIDTH, SCR_HEIGHT, "Minceraft", NULL, NULL);
    if (window == NULL)
    {
        std::cout << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return -1;
    }
    glfwMakeContextCurrent(window);
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
    // glfwSetCursorPosCallback(window, mouse_callback); // Disable callback
    // glfwSetScrollCallback(window, scroll_callback);

    // tell GLFW to capture our mouse
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

    // glew: load all OpenGL function pointers
    // ---------------------------------------
    #ifdef USE_GLEW
    glewExperimental = GL_TRUE;
    if (glewInit() != GLEW_OK)
    {
        std::cout << "Failed to initialize GLEW" << std::endl;
        return -1;
    }
    #endif

    // configure global opengl state
    // -----------------------------
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glCullFace(GL_BACK);

    // Setup ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;       // Enable Keyboard Controls
    //io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;           // Enable Docking
    io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;         // Enable Multi-Viewport / Platform Windows
    //io.ConfigViewportsNoAutoMerge = true;
    //io.ConfigViewportsNoTaskBarIcon = true;

    // Setup Dear ImGui style
    ImGui::StyleColorsDark();
    //ImGui::StyleColorsClassic();

    // When viewports are enabled we tweak WindowRounding/WindowBg so platform windows can look identical to regular ones.
    ImGuiStyle& style = ImGui::GetStyle();
    if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
    {
        style.WindowRounding = 0.0f;
        style.Colors[ImGuiCol_WindowBg].w = 1.0f;
    }

    // Setup Platform/Renderer backends
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    const char* glsl_version = "#version 130";
    ImGui_ImplOpenGL3_Init(glsl_version);

    // build and compile our shader zprogram
    // ------------------------------------
    Shader ourShader("src/shaders/basic.vs", "src/shaders/basic.fs");
    
    // load texture
    // Generate Procedural Atlas using TextureAtlas class
    TextureAtlas atlas(64, 64, 16);
    atlas.Generate();
    
    Texture blockTexture(atlas.GetWidth(), atlas.GetHeight(), atlas.GetData(), 3);
    // tell opengl for each sampler to which texture unit it belongs to
    ourShader.use();
    ourShader.setInt("texture1", 0);

    // World generation
    World world;
    WorldGenerator generator;
    
    // Initial Load
    // Dummy matrix or calculate initial
    glm::mat4 initProj = glm::perspective(glm::radians(camera.Zoom), (float)SCR_WIDTH / (float)SCR_HEIGHT, 0.1f, 1000.0f);
    glm::mat4 initView = camera.GetViewMatrix();
    world.loadChunks(player.Position, dbg_renderDistance, initProj * initView);
    
    // Wait for initial chunks to spawn to avoid falling into void?
    // For now, let's just let it load asynchronously.
    
    /*
    std::vector<Chunk*> allChunks;
    for(int x = -worldSize; x < worldSize; ++x)
    ...
    // Removed old manual generation code
    */

    // Safe Spawn Calculation
    // Pre-Spawn Generation Loop
    // Ensure the spawn chunks are generated so we can find the ground.
    int spawnX = 8;
    int spawnZ = 8;
    float spawnY = 85.0f; // Default safe air drop

    // Camera/Player are initialized with default.
    // Set Player Y high to prioritize surface chunks during loadChunks
    player.Position.y = 100.0f; 

    // We need to drive the world generation for a few frames.
    
    bool foundGround = false;
    int retry = 0;
    const int MAX_RETRIES = 500; // 5 seconds approx

    std::cout << "Generating Spawn Area..." << std::endl;

    while(!foundGround && retry < MAX_RETRIES) {
        // Drive World Generation
        // Use a small render distance for spawn
        // We need a dummy viewProjection
        glm::mat4 view = camera.GetViewMatrix();
        glm::mat4 projection = glm::perspective(glm::radians(camera.Zoom), (float)SCR_WIDTH / (float)SCR_HEIGHT, 0.1f, 1000.0f);
        world.loadChunks(player.Position, 4, projection * view); 
        world.Update();

        // Check columns from top down
        
        // Critical: Wait for the expected surface chunk (Chunk Y=4 -> 64-80) to be loaded.
        // If it's missing, we are definitely not ready.
        int cx = (spawnX >= 0) ? (spawnX / CHUNK_SIZE) : ((spawnX + 1) / CHUNK_SIZE - 1);
        int cz = (spawnZ >= 0) ? (spawnZ / CHUNK_SIZE) : ((spawnZ + 1) / CHUNK_SIZE - 1);
        
        if(world.getChunk(cx, 4, cz) == nullptr) {
            // Surface chunk missing, keep loading
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            retry++;
            continue;
        }

        // World Height is 256
        for(int y = 255; y > 0; --y) {
             int cy = y / CHUNK_SIZE;
             
             if(world.getChunk(cx, cy, cz) != nullptr) {
                 ChunkBlock b = world.getBlock(spawnX, y, spawnZ);
                 if(b.isActive()) {
                     spawnY = (float)y + 2.5f;
                     foundGround = true;
                     std::cout << "Spawn Ground Found at Y=" << y << std::endl;
                     break;
                 }
             } else {
                 // Ignore missing high chunks
             }
        }
        
        if(!foundGround) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            retry++;
        }
    }
    
    if(!foundGround) {
        std::cout << "Spawn Ground NOT Found (Timeout). Using Air Drop." << std::endl;
    }
    // Debug Title
    // std::string title = "Minceraft - SpawnY: " + std::to_string(spawnY) + (foundGround ? " (Found)" : " (Default)");
    // glfwSetWindowTitle(window, title.c_str());

    // Reset Player and Camera to safe spawn (Center of block!)
    // +0.5f ensures we are not on the corner/edge of blocks
    player.Position = glm::vec3((float)spawnX + 0.5f, spawnY, (float)spawnZ + 0.5f);
    camera.Position = player.GetEyePosition();

    // Crosshair Setup
    float crosshairVertices[] = {
        -0.025f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f,
         0.025f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f,
         0.0f, -0.025f, 0.0f, 1.0f, 1.0f, 1.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f,
         0.0f,  0.025f, 0.0f, 1.0f, 1.0f, 1.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f
    };
    unsigned int crosshairVAO, crosshairVBO;
    glGenVertexArrays(1, &crosshairVAO);
    glGenBuffers(1, &crosshairVBO);
    glBindVertexArray(crosshairVAO);
    glBindBuffer(GL_ARRAY_BUFFER, crosshairVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(crosshairVertices), crosshairVertices, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 11 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 11 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);
    // Lighting
    glVertexAttribPointer(3, 3, GL_FLOAT, GL_FALSE, 11 * sizeof(float), (void*)(8 * sizeof(float)));
    glEnableVertexAttribArray(3);

    // Selection Box Setup
    unsigned int selectVAO, selectVBO;
    glGenVertexArrays(1, &selectVAO);
    glGenBuffers(1, &selectVBO);
    glBindVertexArray(selectVAO);
    glBindBuffer(GL_ARRAY_BUFFER, selectVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(float)*24*11, NULL, GL_DYNAMIC_DRAW); // 12 lines * 2 verts * 11 stride
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 11 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 11 * sizeof(float), (void*)(3*sizeof(float)));
    glEnableVertexAttribArray(1);
    // Lighting
    glVertexAttribPointer(3, 3, GL_FLOAT, GL_FALSE, 11 * sizeof(float), (void*)(8 * sizeof(float)));
    glEnableVertexAttribArray(3);

    bool lastLeftMouse = false;
bool lastRightMouse = false;
BlockType selectedBlock = STONE;

    // Global Time
    float globalTime = 0.0f;
    
    // Fixed Time Step
    float tickAccumulator = 0.0f;
    const float TICK_RATE = 20.0f;
    const float TICK_INTERVAL = 1.0f / TICK_RATE;
    
    // render loop
    // -----------
    while (!glfwWindowShouldClose(window))
    {
        // per-frame time logic
        // --------------------
        float currentFrame = static_cast<float>(glfwGetTime());
        deltaTime = currentFrame - lastFrame;
        lastFrame = currentFrame;
        deltaTime = std::min(deltaTime, 0.1f); // Clamp
        
        if(!dbg_timePaused) {
            globalTime += deltaTime * dbg_timeSpeed;
        }

        // Start the Dear ImGui frame
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        
        tickAccumulator += deltaTime;
        while(tickAccumulator >= TICK_INTERVAL) {
            world.Tick();
            tickAccumulator -= TICK_INTERVAL;
        }

        // World Update (Mesh Uploads)
        world.Update();
        
        // LOD Check (Every 0.5s)
        static float lodTimer = 0.0f;
        lodTimer += deltaTime;
        if(lodTimer > 0.5f) {

            lodTimer = 0.0f;
            glm::mat4 projection = glm::perspective(glm::radians(camera.Zoom), (float)SCR_WIDTH / (float)SCR_HEIGHT, 0.1f, 1000.0f);
            glm::mat4 view = camera.GetViewMatrix();
            world.loadChunks(player.Position, dbg_renderDistance, projection * view);
        }

        // Calculate Sun Brightness
        // Simple Sine wave day/night cycle
        // Cycle length: 2400 seconds
        // Factor = 2*PI / 2400 = PI / 1200
        const float cycleFactor = 3.14159265f / 1200.0f;
        float sunStrength = (sin(globalTime * cycleFactor) + 1.0f) * 0.5f; 
        // Clamp minimum brightness so it's not pitch black (moonlight)
        sunStrength = std::max(0.05f, sunStrength);

        // Interaction (Raycast)
        glm::ivec3 hitPos;
        glm::ivec3 prePos;
        bool hit = world.raycast(camera.Position, camera.Front, 5.0f, hitPos, prePos);

        // Debug Window
        if (isDebugMode) {
            ImGui::Begin("Debug Info");
            
            if (ImGui::CollapsingHeader("Stats", ImGuiTreeNodeFlags_DefaultOpen)) {
                ImGui::Text("FPS: %.1f (%.3f ms)", ImGui::GetIO().Framerate, 1000.0f / ImGui::GetIO().Framerate);
                
                // Plot Lines
                dbg_frametimes[dbg_frametime_offset] = deltaTime * 1000.0f;
                dbg_frametime_offset = (dbg_frametime_offset + 1) % 120;
                ImGui::PlotLines("Frame Time", dbg_frametimes, 120, dbg_frametime_offset, "ms", 0.0f, 50.0f, ImVec2(0, 80));

                ImGui::Separator();
                ImGui::Text("Position: %.2f, %.2f, %.2f", player.Position.x, player.Position.y, player.Position.z);
                ImGui::Text("Velocity: %.2f, %.2f, %.2f", player.Velocity.x, player.Velocity.y, player.Velocity.z);
                ImGui::Text("Yaw: %.1f, Pitch: %.1f", player.Yaw, player.Pitch);
                ImGui::Text("Grounded: %s", player.IsGrounded ? "Yes" : "No");
                
                ImGui::Separator();
                ImGui::Text("Sun Strength: %.2f", sunStrength);
                ImGui::Text("Chunks: %d / %zu", dbg_renderedChunks, world.getChunkCount());
                ImGui::Text("Tick: %lld", world.currentTick);
            }

            if (ImGui::CollapsingHeader("Controls", ImGuiTreeNodeFlags_DefaultOpen)) {
                 // Teleport
                 if (ImGui::Button("Teleport")) {
                     player.Position = glm::vec3(dbg_teleport_pos[0], dbg_teleport_pos[1], dbg_teleport_pos[2]);
                     // Reset velocity to avoid carrying momentum into wall
                     player.Velocity = glm::vec3(0.0f); 
                 }
                 ImGui::SameLine();
                 ImGui::InputFloat3("##pos", dbg_teleport_pos);
                 
                 // Current Pos to Teleport Target
                 if(ImGui::Button("Copy Current Pos")) {
                     dbg_teleport_pos[0] = player.Position.x;
                     dbg_teleport_pos[1] = player.Position.y;
                     dbg_teleport_pos[2] = player.Position.z;
                 }

                 ImGui::Separator();
                 // FOV
                 ImGui::SliderFloat("FOV", &camera.Zoom, 1.0f, 120.0f);
                 
                 // VSync
                 if(ImGui::Checkbox("VSync", &dbg_vsync)) {
                     glfwSwapInterval(dbg_vsync ? 1 : 0);
                 }

                 ImGui::Separator();
                 ImGui::Text("Time Controls");
                 if(ImGui::Button(dbg_timePaused ? "Resume" : "Pause")) {
                     dbg_timePaused = !dbg_timePaused;
                 }
                 ImGui::SameLine();
                 ImGui::SliderFloat("Speed", &dbg_timeSpeed, 0.0f, 10.0f);
                 ImGui::SliderFloat("Time", &globalTime, 0.0f, 2400.0f); // 2400s cycle
                 
                 ImGui::Separator();
                 ImGui::Text("Player / Render");
                 ImGui::Checkbox("Fly Mode (Noclip)", &player.FlyMode);
                 ImGui::Checkbox("Wireframe", &dbg_wireframe);
                 if(ImGui::SliderInt("Render Dist", &dbg_renderDistance, 2, 32)) {
                     glm::mat4 proj = glm::perspective(glm::radians(camera.Zoom), (float)SCR_WIDTH / (float)SCR_HEIGHT, 0.1f, 1000.0f);
                     glm::mat4 view = camera.GetViewMatrix();
                     world.loadChunks(player.Position, dbg_renderDistance, proj * view);
                 }
                 ImGui::SliderFloat("Gravity", &player.Gravity, 0.0f, 50.0f);
                 
                 ImGui::SameLine();
                 ImGui::Checkbox("Freeze Culling", &dbg_freezeCulling);
                 
                 ImGui::Separator();
                 ImGui::Text("Visualization");
                 ImGui::Checkbox("Chunk Borders", &dbg_chunkBorders);
                 ImGui::Checkbox("Light Heatmap", &dbg_useHeatmap);
                 ImGui::Checkbox("Fog", &dbg_useFog);
                 if(dbg_useFog) {
                     ImGui::SliderFloat("Fog Dist", &dbg_fogDist, 10.0f, 200.0f);
                 }
            }
            
            if (ImGui::CollapsingHeader("Creative Menu", ImGuiTreeNodeFlags_DefaultOpen)) {
                // Grid of blocks
                int buttonsPerRow = 4;
                for(int i=1; i<=10; ++i) { // 1 to 10 are valid blocks
                    if(i > 1 && (i-1) % buttonsPerRow != 0) ImGui::SameLine();
                    
                    std::string label = GetBlockName(i);
                    if(ImGui::Button((label + "##btn").c_str(), ImVec2(60, 60))) {
                        selectedBlock = (BlockType)i;
                    }
                    
                    // Highlight selected
                    if((int)selectedBlock == i) {
                        ImGui::GetWindowDrawList()->AddRect(
                            ImGui::GetItemRectMin(), ImGui::GetItemRectMax(), 
                            IM_COL32(255, 255, 0, 255), 3.0f);
                    }
                }
                ImGui::Text("Selected: %s", GetBlockName(selectedBlock));
            }
            
            if (ImGui::CollapsingHeader("Raycast", ImGuiTreeNodeFlags_DefaultOpen)) {
                if(hit) {
                    ChunkBlock cb = world.getBlock(hitPos.x, hitPos.y, hitPos.z);
                    ImGui::Text("Hit Block: %s (%d)", GetBlockName(cb.getType()), cb.getType());
                    ImGui::Text("Hit Pos: %d, %d, %d", hitPos.x, hitPos.y, hitPos.z);
                    ImGui::Text("Pre Pos: %d, %d, %d", prePos.x, prePos.y, prePos.z);
                    
                    uint8_t sl = world.getSkyLight(hitPos.x, hitPos.y, hitPos.z);
                    uint8_t bl = world.getBlockLight(hitPos.x, hitPos.y, hitPos.z);
                    ImGui::Text("Light: Sky %d, Block %d", sl, bl);

                } else {
                    ImGui::Text("No Hit");
                }
            }

            ImGui::Separator();
            #ifdef MINCERAFT_DEBUG
            ImGui::TextColored(ImVec4(1,1,0,1), "DEBUG BUILD");
            #else
            ImGui::TextColored(ImVec4(0,1,0,1), "RELEASE BUILD");
            #endif

            ImGui::End();
        }

        // input
        // -----
        processInput(window, world);
        
        // Update Player Physics
        player.Update(deltaTime, world);
        
        // Sync Camera
        camera.Position = player.Position;
        camera.Front = player.Front;
        camera.Up = player.Up;

        processInput(window, world);

        // Render
        // ------
        
        // Water Tint Logic
        bool inWater = false;
        ChunkBlock camBlock = world.getBlock((int)floor(camera.Position.x), (int)floor(camera.Position.y), (int)floor(camera.Position.z));
        if(camBlock.getType() == WATER || camBlock.getType() == LAVA) inWater = true;
        
        glm::vec3 skyColor = glm::vec3(0.2f, 0.3f, 0.3f) * sunStrength;
        glm::vec3 fogCol = glm::vec3(0.5f, 0.6f, 0.7f) * sunStrength;
        float fDist = dbg_fogDist;
        bool uFog = dbg_useFog;
        
        if(inWater) {
            // Underwater Blue
            if(camBlock.getType() == WATER) {
                skyColor = glm::vec3(0.1f, 0.1f, 0.4f) * sunStrength;
                fogCol = glm::vec3(0.05f, 0.05f, 0.3f) * sunStrength; 
            } else {
                // Lava Red
                skyColor = glm::vec3(0.6f, 0.1f, 0.0f);
                fogCol = glm::vec3(0.5f, 0.0f, 0.0f);
            }
            
            fDist = 15.0f; // Very close fog
            uFog = true;   // Force fog on
        }

        glClearColor(skyColor.x, skyColor.y, skyColor.z, 1.0f);
        glClear(GL_DEPTH_BUFFER_BIT | GL_COLOR_BUFFER_BIT); 

        // activate shader
        ourShader.use();
        ourShader.setFloat("sunStrength", sunStrength);
        ourShader.setVec3("viewPos", camera.Position);
        ourShader.setBool("useHeatmap", dbg_useHeatmap);
        ourShader.setBool("useFog", uFog);
        ourShader.setFloat("fogDist", fDist);
        ourShader.setVec3("fogColor", fogCol);

        // pass projection matrix to shader (note: in this case it could change every frame)
        glm::mat4 projection = glm::perspective(glm::radians(camera.Zoom), (float)SCR_WIDTH / (float)SCR_HEIGHT, 0.1f, 1000.0f);
        ourShader.setMat4("projection", projection);

        // camera/view transformation
        glm::mat4 view = camera.GetViewMatrix();
        ourShader.setMat4("view", view);

        // render chunk
        ourShader.setBool("useTexture", true);
        blockTexture.bind();
        
        if(dbg_wireframe) glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
        
        glm::mat4 cullMatrix = projection * view;
        if(dbg_freezeCulling) {
             cullMatrix = dbg_frozenProjView;
        } else {
             dbg_frozenProjView = cullMatrix;
        }
        
        dbg_renderedChunks = world.render(ourShader, cullMatrix);
        
        if(dbg_wireframe) glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
        
        if(dbg_chunkBorders) {
            world.renderDebugBorders(ourShader, projection * view);
        }



        if(hit) {
            // Destruction
            bool currentLeftMouse = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS;
            if(currentLeftMouse && !lastLeftMouse) {
                world.setBlock(hitPos.x, hitPos.y, hitPos.z, AIR);
            }


            lastLeftMouse = currentLeftMouse;

            // Draw Selection Wireframe
            float gap = 0.001f;
            float minX = hitPos.x - gap; float maxX = hitPos.x + 1 + gap;
            float minY = hitPos.y - gap; float maxY = hitPos.y + 1 + gap;
            float minZ = hitPos.z - gap; float maxZ = hitPos.z + 1 + gap;
            // White lines
            float r=1.0f, g=1.0f, b=1.0f;
            float boxVerts[] = {
                 minX, minY, minZ, r,g,b, 0,0, 1,1,1,  maxX, minY, minZ, r,g,b, 0,0, 1,1,1,
                 maxX, minY, minZ, r,g,b, 0,0, 1,1,1,  maxX, maxY, minZ, r,g,b, 0,0, 1,1,1,
                 maxX, maxY, minZ, r,g,b, 0,0, 1,1,1,  minX, maxY, minZ, r,g,b, 0,0, 1,1,1,
                 minX, maxY, minZ, r,g,b, 0,0, 1,1,1,  minX, minY, minZ, r,g,b, 0,0, 1,1,1,
                 
                 minX, minY, maxZ, r,g,b, 0,0, 1,1,1,  maxX, minY, maxZ, r,g,b, 0,0, 1,1,1,
                 maxX, minY, maxZ, r,g,b, 0,0, 1,1,1,  maxX, maxY, maxZ, r,g,b, 0,0, 1,1,1,
                 maxX, maxY, maxZ, r,g,b, 0,0, 1,1,1,  minX, maxY, maxZ, r,g,b, 0,0, 1,1,1,
                 minX, maxY, maxZ, r,g,b, 0,0, 1,1,1,  minX, minY, maxZ, r,g,b, 0,0, 1,1,1,
                 
                 minX, minY, minZ, r,g,b, 0,0, 1,1,1,  minX, minY, maxZ, r,g,b, 0,0, 1,1,1,
                 maxX, minY, minZ, r,g,b, 0,0, 1,1,1,  maxX, minY, maxZ, r,g,b, 0,0, 1,1,1,
                 maxX, maxY, minZ, r,g,b, 0,0, 1,1,1,  maxX, maxY, maxZ, r,g,b, 0,0, 1,1,1,
                 minX, maxY, minZ, r,g,b, 0,0, 1,1,1,  minX, maxY, maxZ, r,g,b, 0,0, 1,1,1
            };
            
            glBindVertexArray(selectVAO);
            glBindBuffer(GL_ARRAY_BUFFER, selectVBO);
            glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(boxVerts), boxVerts);
            
            ourShader.setMat4("model", glm::mat4(1.0f));
            ourShader.setBool("useTexture", false);
            glDrawArrays(GL_LINES, 0, 24);
        } else {
             lastLeftMouse = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS;
        }

        // Right Mouse - Placement
        bool currentRightMouse = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS;
        if(hit && currentRightMouse && !lastRightMouse)
        {
            // Use prePos for placement
            int placeX = prePos.x;
            int placeY = prePos.y;
            int placeZ = prePos.z;

            ChunkBlock targetBlock = world.getBlock(placeX, placeY, placeZ);
            bool canPlace = !targetBlock.isActive() || targetBlock.getType() == WATER || targetBlock.getType() == LAVA;

            if (canPlace)
            {
                // Check if we stuck the player (PRE-CHECK)
                // We do this BEFORE setBlock to avoid "Ghost Light" bugs where reverting (setBlock to AIR)
                // fails to fully clean up lighting propagated to neighbors.
                
                float playerWidth = 0.6f;
                float playerHeight = 1.8f;
                float eyeHeight = 1.6f;
                float epsilon = 0.05f;
                
                float minX = player.Position.x - playerWidth / 2.0f;
                float maxX = player.Position.x + playerWidth / 2.0f;
                float minY = player.Position.y - eyeHeight + epsilon;
                float maxY = player.Position.y - eyeHeight + playerHeight - epsilon;
                float minZ = player.Position.z - playerWidth / 2.0f;
                float maxZ = player.Position.z + playerWidth / 2.0f;
                
                bool intersects = (maxX > placeX && minX < placeX + 1) &&
                                  (maxY > placeY && minY < placeY + 1) &&
                                  (maxZ > placeZ && minZ < placeZ + 1);

                if (!intersects) {
                    world.setBlock(placeX, placeY, placeZ, selectedBlock);
                } else {
                    // Collision detected, do not place block.
                }
            }
        }
        lastRightMouse = currentRightMouse;
        
        // Inventory Selection
        if(glfwGetKey(window, GLFW_KEY_1) == GLFW_PRESS) selectedBlock = DIRT;
        if(glfwGetKey(window, GLFW_KEY_2) == GLFW_PRESS) selectedBlock = STONE;
        if(glfwGetKey(window, GLFW_KEY_3) == GLFW_PRESS) selectedBlock = GRASS;
        if(glfwGetKey(window, GLFW_KEY_4) == GLFW_PRESS) selectedBlock = WOOD;
        if(glfwGetKey(window, GLFW_KEY_5) == GLFW_PRESS) selectedBlock = LEAVES;
        if(glfwGetKey(window, GLFW_KEY_6) == GLFW_PRESS) selectedBlock = COAL_ORE;
        if(glfwGetKey(window, GLFW_KEY_7) == GLFW_PRESS) selectedBlock = IRON_ORE;
        if(glfwGetKey(window, GLFW_KEY_8) == GLFW_PRESS) selectedBlock = GLOWSTONE;

        // Draw Crosshair
        float aspect = (float)SCR_WIDTH / (float)SCR_HEIGHT;
        glm::mat4 crosshairModel = glm::scale(glm::mat4(1.0f), glm::vec3(1.0f/aspect, 1.0f, 1.0f));
        ourShader.setMat4("model", crosshairModel);
        ourShader.setMat4("view", glm::mat4(1.0f));
        ourShader.setMat4("projection", glm::mat4(1.0f));
        ourShader.setBool("useTexture", false);
        
        glBindVertexArray(crosshairVAO);
        glDrawArrays(GL_LINES, 0, 4);


        // glfw: swap buffers and poll IO events (keys pressed/released, mouse moved etc.)
        // -------------------------------------------------------------------------------
        // Render ImGui
        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        
        // Update and Render additional Platform Windows
        // (Platform functions may change the current OpenGL context, so we save/restore it to make it easier to paste this code elsewhere.
        //  For this specific demo app we could also call glfwMakeContextCurrent(window) directly)
        ImGuiIO& io = ImGui::GetIO(); (void)io;
        if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
        {
            GLFWwindow* backup_current_context = glfwGetCurrentContext();
            ImGui::UpdatePlatformWindows();
            ImGui::RenderPlatformWindowsDefault();
            glfwMakeContextCurrent(backup_current_context);
        }

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    // optional: de-allocate all resources once they've outlived their purpose:
    // ------------------------------------------------------------------------
    // chunk cleans up itself


    // glfw: terminate, clearing all previously allocated GLFW resources.
    // ------------------------------------------------------------------
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glfwTerminate();
    return 0;
}

// process all input: query GLFW whether relevant keys are pressed/released this frame and react accordingly
// ---------------------------------------------------------------------------------------------------------

void processInput(GLFWwindow *window, const World& world)
{
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        glfwSetWindowShouldClose(window, true);
    
    // Toggle Debug Mode
    bool currentM = glfwGetKey(window, GLFW_KEY_M) == GLFW_PRESS;
    if (currentM && !lastM) {
        isDebugMode = !isDebugMode;
        if (isDebugMode) {
            glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
        } else {
            glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
            firstMouse = true; // Reset mouse look to avoid jumps
        }
    }
    lastM = currentM;
    
    // If in debug mode, return early or skip player controls (except movement maybe?)
    // Let's keep movement but disable mouse look.
    
    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
        player.ProcessKeyboard(P_FORWARD, deltaTime, world);
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
        player.ProcessKeyboard(P_BACKWARD, deltaTime, world);
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
        player.ProcessKeyboard(P_LEFT, deltaTime, world);
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
        player.ProcessKeyboard(P_RIGHT, deltaTime, world);
        
    if (player.FlyMode) {
        if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS)
            player.ProcessKeyboard(P_UP, deltaTime, world);
        if (glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS)
            player.ProcessKeyboard(P_DOWN, deltaTime, world);
    } else {
        if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS)
            player.ProcessJump(true, world);
    }

    // Mouse Polling
    if (!isDebugMode) {
        double xpos, ypos;
        glfwGetCursorPos(window, &xpos, &ypos);

        if (firstMouse)
        {
            lastX = (float)xpos;
            lastY = (float)ypos;
            firstMouse = false;
        }

        float xoffset = (float)xpos - lastX;
        float yoffset = lastY - (float)ypos; // reversed since y-coordinates go from bottom to top

        lastX = (float)xpos;
        lastY = (float)ypos;

        player.ProcessMouseMovement(xoffset, yoffset);
    }
}

// glfw: whenever the window size changed (by OS or user resize) this callback function executes
// ---------------------------------------------------------------------------------------------
void framebuffer_size_callback(GLFWwindow* window, int width, int height)
{
    glViewport(0, 0, width, height);
    SCR_WIDTH = width;
    SCR_HEIGHT = height;
}


