#include <iostream>

#ifdef USE_GLEW
#include <GL/glew.h>
#endif

#include <GLFW/glfw3.h>

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
const unsigned int SCR_WIDTH = 1280;
const unsigned int SCR_HEIGHT = 720;

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
    
    int worldSize = 4; // 8x8 chunks
    int worldHeight = 4; // 0..3 chunks vertically (64 blocks high)
    
    for(int x = -worldSize; x < worldSize; ++x)
    {
        for(int z = -worldSize; z < worldSize; ++z)
        {
            for(int y = 0; y < worldHeight; ++y)
            {
                world.addChunk(x, y, z);
                Chunk* c = world.getChunk(x, y, z);
                if(c)
                    generator.GenerateChunk(*c);
            }
        }
    }

    // Safe Spawn Calculation
    int spawnX = 8;
    int spawnZ = 8;
    float spawnY = 40.0f; // Default fallback

    // Probe the ACTUAL world data to find ground
    bool foundGround = false;
    for(int y = worldHeight * CHUNK_SIZE - 1; y >= 0; --y) {
         if(world.getBlock(spawnX, y, spawnZ).isActive()) {
             spawnY = (float)y + 2.5f;
             foundGround = true;
             break;
         }
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
        -0.03f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f,
         0.03f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f,
         0.0f, -0.04f, 0.0f, 1.0f, 1.0f, 1.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f,
         0.0f,  0.04f, 0.0f, 1.0f, 1.0f, 1.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f
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

        // render
        // ------
        glClearColor(0.2f, 0.3f, 0.3f, 1.0f);
        glClear(GL_DEPTH_BUFFER_BIT | GL_COLOR_BUFFER_BIT); 

        // activate shader
        ourShader.use();

        // pass projection matrix to shader (note: in this case it could change every frame)
        glm::mat4 projection = glm::perspective(glm::radians(camera.Zoom), (float)SCR_WIDTH / (float)SCR_HEIGHT, 0.1f, 100.0f);
        ourShader.setMat4("projection", projection);

        // camera/view transformation
        glm::mat4 view = camera.GetViewMatrix();
        ourShader.setMat4("view", view);

        // render chunk
        ourShader.setBool("useTexture", true);
        blockTexture.bind();
        world.render(ourShader);

        // Interaction
        glm::ivec3 hitPos;
        glm::ivec3 prePos;
        bool hit = world.raycast(camera.Position, camera.Front, 5.0f, hitPos, prePos);

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

            if (world.getBlock(placeX, placeY, placeZ).isActive() == false)
            {
                // Try to place block
                world.setBlock(placeX, placeY, placeZ, selectedBlock);
                
                // Check if we stuck the player
                if(player.CheckCollision(player.Position, world))
                {
                    // Revert
                     world.setBlock(placeX, placeY, placeZ, (BlockType)0); 
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

        // Draw Crosshair (Identity Matrix)
        ourShader.setMat4("model", glm::mat4(1.0f));
        ourShader.setMat4("view", glm::mat4(1.0f));
        ourShader.setMat4("projection", glm::mat4(1.0f));
        ourShader.setBool("useTexture", false);
        
        glBindVertexArray(crosshairVAO);
        glDrawArrays(GL_LINES, 0, 4);


        // glfw: swap buffers and poll IO events (keys pressed/released, mouse moved etc.)
        // -------------------------------------------------------------------------------
        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    // optional: de-allocate all resources once they've outlived their purpose:
    // ------------------------------------------------------------------------
    // chunk cleans up itself


    // glfw: terminate, clearing all previously allocated GLFW resources.
    // ------------------------------------------------------------------
    glfwTerminate();
    return 0;
}

// process all input: query GLFW whether relevant keys are pressed/released this frame and react accordingly
// ---------------------------------------------------------------------------------------------------------
void processInput(GLFWwindow *window, const World& world)
{
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        glfwSetWindowShouldClose(window, true);

    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
        player.ProcessKeyboard(P_FORWARD, deltaTime, world);
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
        player.ProcessKeyboard(P_BACKWARD, deltaTime, world);
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
        player.ProcessKeyboard(P_LEFT, deltaTime, world);
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
        player.ProcessKeyboard(P_RIGHT, deltaTime, world);
    if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS)
        player.ProcessJump();

    // Mouse Polling
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

// glfw: whenever the window size changed (by OS or user resize) this callback function executes
// ---------------------------------------------------------------------------------------------
void framebuffer_size_callback(GLFWwindow* window, int width, int height)
{
    glViewport(0, 0, width, height);
}


