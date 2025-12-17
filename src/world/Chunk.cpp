#include "Chunk.h"
#include "World.h"
#include <glm/gtc/matrix_transform.hpp>
#include <queue>
#include <tuple>

Chunk::Chunk() : meshDirty(true), indexCount(0), chunkPosition(0,0,0), world(nullptr)
{
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);
    glGenBuffers(1, &EBO);
    // Initialize with air
    for(int x=0; x<CHUNK_SIZE; ++x)
        for(int y=0; y<CHUNK_SIZE; ++y)
            for(int z=0; z<CHUNK_SIZE; ++z)
                blocks[x][y][z].type = AIR;
}

Chunk::~Chunk()
{
    glDeleteVertexArrays(1, &VAO);
    glDeleteBuffers(1, &VBO);
    glDeleteBuffers(1, &EBO);
}


Block Chunk::getBlock(int x, int y, int z) const
{
    if(x < 0 || x >= CHUNK_SIZE || y < 0 || y >= CHUNK_SIZE || z < 0 || z >= CHUNK_SIZE)
        return {AIR};
    return blocks[x][y][z];
}

void Chunk::setBlock(int x, int y, int z, BlockType type)
{
    if(x < 0 || x >= CHUNK_SIZE || y < 0 || y >= CHUNK_SIZE || z < 0 || z >= CHUNK_SIZE)
        return;
    blocks[x][y][z].type = type;
    meshDirty = true;
}

uint8_t Chunk::getLight(int x, int y, int z) const
{
    if(x < 0 || x >= CHUNK_SIZE || y < 0 || y >= CHUNK_SIZE || z < 0 || z >= CHUNK_SIZE)
        return 0;
    return blocks[x][y][z].light;
}

void Chunk::updateMesh()
{
    vertices.clear();
    indices.clear();
    indexCount = 0;
    
    unsigned int vIndex = 0;
    
    // -------------------------------------------------------
    // Lighting Calculation (BFS)
    // -------------------------------------------------------
    // 1. Reset Light
    for(int x=0; x<CHUNK_SIZE; ++x)
        for(int y=0; y<CHUNK_SIZE; ++y)
            for(int z=0; z<CHUNK_SIZE; ++z)
                blocks[x][y][z].light = 0;

    std::queue<glm::ivec3> lightQueue;

    // 2. Seed Sunlight
    for(int x=0; x<CHUNK_SIZE; ++x) {
        for(int z=0; z<CHUNK_SIZE; ++z) {
            // Check world column above chunk
            bool skyVisible = true;
            if(world) {
                int gx = chunkPosition.x * CHUNK_SIZE + x;
                int gy_top = chunkPosition.y * CHUNK_SIZE + CHUNK_SIZE; // Start just above chunk
                int gz = chunkPosition.z * CHUNK_SIZE + z;
                
                // Heuristic check up to height 32
                for(int cy = gy_top; cy < 32; ++cy) {
                    if(world->getBlock(gx, cy, gz).isActive()) {
                        skyVisible = false; 
                        break;
                    }
                }
            }

            if(skyVisible) {
                // Propagate down in this chunk
                for(int y=CHUNK_SIZE-1; y>=0; --y) {
                    if(blocks[x][y][z].isActive()) {
                        break; // Hit solid
                    } else {
                        blocks[x][y][z].light = 15;
                        lightQueue.push(glm::ivec3(x, y, z));
                    }
                }
            }
        }
    }


    // 2b. Seed from Neighbor Chunks (Cross-Chunk Propagation)
    if(world) {
        int cx = chunkPosition.x;
        int cy = chunkPosition.y;
        int cz = chunkPosition.z;

        struct Neighbor { int dx, dy, dz; int ox, oy, oz; int faceAxis; };
        Neighbor neighbors[] = {
            {-1, 0, 0,  CHUNK_SIZE-1, 0, 0,   0}, // Left Neighbor (Check its Right face x=15)
            { 1, 0, 0,  0, 0, 0,              0}, // Right Neighbor (Check its Left face x=0)
            { 0,-1, 0,  0, CHUNK_SIZE-1, 0,   1}, // Bottom
            { 0, 1, 0,  0, 0, 0,              1}, // Top
            { 0, 0,-1,  0, 0, CHUNK_SIZE-1,   2}, // Back
            { 0, 0, 1,  0, 0, 0,              2}  // Front
        };

        for(const auto& n : neighbors) {
            Chunk* nc = world->getChunk(cx + n.dx, cy + n.dy, cz + n.dz);
            if(nc) {
                // Iterate the interface face
                // Axis 0 (X): Iterate Y, Z
                // Axis 1 (Y): Iterate X, Z
                // Axis 2 (Z): Iterate X, Y
                
                for(int u=0; u<CHUNK_SIZE; ++u) {
                    for(int v=0; v<CHUNK_SIZE; ++v) {
                        int lx, ly, lz; // Local coords in THIS chunk to update
                        int nx, ny, nz; // Neighbor coords to read from

                        if(n.faceAxis == 0) { // X-face
                            lx = (n.dx == -1) ? 0 : CHUNK_SIZE-1;
                            ly = u; lz = v;
                            nx = n.ox; ny = u; nz = v;
                        } else if(n.faceAxis == 1) { // Y-face
                            lx = u; ly = (n.dy == -1) ? 0 : CHUNK_SIZE-1; lz = v;
                            nx = u; ny = n.oy; nz = v;
                        } else { // Z-face
                            lx = u; ly = v; lz = (n.dz == -1) ? 0 : CHUNK_SIZE-1;
                            nx = u; ny = v; nz = n.oz;
                        }

                        if(!blocks[lx][ly][lz].isActive()) {
                             uint8_t nLight = nc->getLight(nx, ny, nz);
                             if(nLight > 1 && nLight - 1 > blocks[lx][ly][lz].light) {
                                 blocks[lx][ly][lz].light = nLight - 1;
                                 lightQueue.push(glm::ivec3(lx, ly, lz));
                             }
                        }
                    }
                }
            }
        }
    }

    // 3. Propagate
    int directions[6][3] = {
        {1,0,0}, {-1,0,0},
        {0,1,0}, {0,-1,0},
        {0,0,1}, {0,0,-1}
    };

    while(!lightQueue.empty()) {
        glm::ivec3 pos = lightQueue.front();
        lightQueue.pop();
        
        int curLight = blocks[pos.x][pos.y][pos.z].light;
        if(curLight <= 1) continue; // Can't spread darkness

        for(int i=0; i<6; ++i) {
            int nx = pos.x + directions[i][0];
            int ny = pos.y + directions[i][1];
            int nz = pos.z + directions[i][2];

            if(nx >= 0 && nx < CHUNK_SIZE && ny >= 0 && ny < CHUNK_SIZE && nz >= 0 && nz < CHUNK_SIZE) {
                if(!blocks[nx][ny][nz].isActive()) { // Air only
                    if(blocks[nx][ny][nz].light < curLight - 1) {
                        blocks[nx][ny][nz].light = curLight - 1;
                        lightQueue.push(glm::ivec3(nx, ny, nz));
                    }
                }
            }
        }
    }
    // -------------------------------------------------------

    for(int x=0; x<CHUNK_SIZE; ++x)
    {
        for(int y=0; y<CHUNK_SIZE; ++y)
        {
            for(int z=0; z<CHUNK_SIZE; ++z)
            {
                BlockType type = (BlockType)blocks[x][y][z].type;
                if(type == AIR) continue;

                // Check neighbors
                // Front (Z+1)
                if(!getBlock(x, y, z+1).isActive()) addFace(x, y, z, 0, type); // Front
                if(!getBlock(x, y, z-1).isActive()) addFace(x, y, z, 1, type); // Back
                if(!getBlock(x-1, y, z).isActive()) addFace(x, y, z, 2, type); // Left
                if(!getBlock(x+1, y, z).isActive()) addFace(x, y, z, 3, type); // Right
                if(!getBlock(x, y+1, z).isActive()) addFace(x, y, z, 4, type); // Top
                if(!getBlock(x, y-1, z).isActive()) addFace(x, y, z, 5, type); // Bottom
            }
        }
    }

    glBindVertexArray(VAO);
    
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(float), vertices.data(), GL_STATIC_DRAW);

    // Use 11 * float for stride (3 pos + 3 color + 2 tex + 3 light)
    float stride = 11 * sizeof(float);
    
    // Position
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, (void*)0);
    glEnableVertexAttribArray(0);
    // Color
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, stride, (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);
    // TexCoord
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, stride, (void*)(6 * sizeof(float)));
    glEnableVertexAttribArray(2);
    // Lighting
    glVertexAttribPointer(3, 3, GL_FLOAT, GL_FALSE, stride, (void*)(8 * sizeof(float)));
    glEnableVertexAttribArray(3);

    // Use Element Buffer for indexed drawing if we were optimizing vertices, but here we just push triangles.
    // Actually, my addFace implementation below will probably just push vertices for GL_TRIANGLES.
    // Let's stick to draw arrays if I don't implement EBO logic for now, or use EBO correctly.
    // I defined indices vector, but let's just use glDrawArrays for simplicity unless I deduplicate vertices.
    // I'll stick to glDrawArrays.
}

void Chunk::addFace(int x, int y, int z, int faceDir, int blockType)
{
    // Colors
    float r, g, b;
    // Set colors for tinting textures
    // Grass: Green Tint (Texture is greyscale)
    if (blockType == GRASS) { r = 0.0f; g = 1.0f; b = 0.0f; } 
    // Dirt: Brown Tint (Texture is greyscale)
    else if (blockType == DIRT) { r = 0.6f; g = 0.4f; b = 0.2f; }
    // Stone: White Tint (Use texture color directly - which is Grey)
    else if (blockType == STONE) { r = 1.0f; g = 1.0f; b = 1.0f; } 
    // Wood uses texture colors (White Tint)
    else if (blockType == WOOD) { r = 1.0f; g = 1.0f; b = 1.0f; } 
    // Leaves: Green Tint
    else if (blockType == LEAVES) { r = 0.2f; g = 0.8f; b = 0.2f; } 
    // Ores: White Tint (Use texture)
    else if (blockType == COAL_ORE || blockType == IRON_ORE) { r = 1.0f; g = 1.0f; b = 1.0f; }
    else { r = 1.0f; g = 0.0f; b = 1.0f; } // Pink error

    // Lighting
    // Get light from the air block next to the face (where the face is looking)
    float lightVal = 1.0f;
    if(world) {
        int gx = chunkPosition.x * CHUNK_SIZE + x;
        int gy = chunkPosition.y * CHUNK_SIZE + y;
        int gz = chunkPosition.z * CHUNK_SIZE + z;
        
        int dx=0, dy=0, dz=0;
        if(faceDir==0) dz=1;      // Front
        else if(faceDir==1) dz=-1;// Back
        else if(faceDir==2) dx=-1;// Left
        else if(faceDir==3) dx=1; // Right
        else if(faceDir==4) dy=1; // Top
        else dy=-1;               // Bottom

        uint8_t l = world->getLight(gx+dx, gy+dy, gz+dz);
        // Base ambient of 1 (so not pitch black)
        if(l < 1) l = 1; 
        lightVal = (float)l / 15.0f;
        
        // Gamma
        lightVal = pow(lightVal, 0.6f); 
    }

    float lr, lg, lb;
    if(faceDir == 4) { lr=1.0f; lg=1.0f; lb=1.0f; } // Top
    else if(faceDir == 5) { lr=0.6f; lg=0.6f; lb=0.6f; } // Bottom
    else { lr=0.8f; lg=0.8f; lb=0.8f; } // Side
    
    // Apply Light
    lr *= lightVal;
    lg *= lightVal;
    lb *= lightVal;

    // (Redundant occlusion logic removed)

    // UV Mapping
    // Atlas 64x64, Blocks 16x16 -> 0.25 step
    // Stone: 0,0
    // Dirt: 1,0
    // Grass: 2,0
    // WoodSide: 0,1
    // WoodTop: 1,1
    // Leaves: 2,1

    float uMin = 0.00f, uMax = 0.25f;
    float vMin = 0.00f, vMax = 0.25f;
    
    if(blockType == DIRT) {
         uMin = 0.25f; uMax = 0.50f;
         vMin = 0.00f; vMax = 0.25f;
    }
    else if(blockType == GRASS) {
         uMin = 0.50f; uMax = 0.75f;
         vMin = 0.00f; vMax = 0.25f;
         if(faceDir == 4) { // Top of Grass (could vary?)
             // For now same
         }
    }
    else if(blockType == WOOD) {
        if(faceDir == 4 || faceDir == 5) { // Top/Bottom
            // Rings (1,1)
            uMin = 0.25f; uMax = 0.50f;
            vMin = 0.25f; vMax = 0.50f;
        } else {
            // Bark (0,1)
            uMin = 0.00f; uMax = 0.25f;
            vMin = 0.25f; vMax = 0.50f;
        }
    } 
    else if(blockType == LEAVES) {
        // Leaves (2,1)
        uMin = 0.50f; uMax = 0.75f;
        vMin = 0.25f; vMax = 0.50f;
    }
    else if(blockType == COAL_ORE) {
        // Coal (3,0)
        uMin = 0.75f; uMax = 1.00f;
        vMin = 0.00f; vMax = 0.25f;
    }
    else if(blockType == IRON_ORE) {
        // Iron (3,1)
        uMin = 0.75f; uMax = 1.00f;
        vMin = 0.25f; vMax = 0.50f;
    }
    // Else Stone uses default 0,0

    float fx = (float)x;
    float fy = (float)y;
    float fz = (float)z;

    // Offsets
    // Front face (z=1)
    if(faceDir == 0) // Front
    {
        float faceData[] = {
            fx, fy, fz+1.0f,    r,g,b,  uMin, vMin,  lr,lg,lb,
            fx+1.0f, fy, fz+1.0f,  r,g,b,  uMax, vMin,  lr,lg,lb,
            fx+1.0f, fy+1.0f, fz+1.0f,r,g,b,  uMax, vMax,  lr,lg,lb,
            fx, fy, fz+1.0f,    r,g,b,  uMin, vMin,  lr,lg,lb,
            fx+1.0f, fy+1.0f, fz+1.0f,r,g,b,  uMax, vMax,  lr,lg,lb,
            fx, fy+1.0f, fz+1.0f,  r,g,b,  uMin, vMax,  lr,lg,lb
        };
        vertices.insert(vertices.end(), faceData, faceData + 66);
    }
    // Back face (z=0)
    else if(faceDir == 1) // Back
    {
        float faceData[] = {
            fx+1.0f, fy, fz,    r,g,b,  uMin, vMin,  lr,lg,lb,
            fx, fy, fz,         r,g,b,  uMax, vMin,  lr,lg,lb,
            fx, fy+1.0f, fz,    r,g,b,  uMax, vMax,  lr,lg,lb,
            fx+1.0f, fy, fz,    r,g,b,  uMin, vMin,  lr,lg,lb,
            fx, fy+1.0f, fz,    r,g,b,  uMax, vMax,  lr,lg,lb,
            fx+1.0f, fy+1.0f, fz,  r,g,b,  uMin, vMax,  lr,lg,lb
        };
        vertices.insert(vertices.end(), faceData, faceData + 66);
    }
    // Left (x=0)
    else if(faceDir == 2)
    {
        float faceData[] = {
            fx, fy, fz,         r,g,b,  uMin, vMin,  lr,lg,lb,
            fx, fy, fz+1.0f,    r,g,b,  uMax, vMin,  lr,lg,lb,
            fx, fy+1.0f, fz+1.0f,  r,g,b,  uMax, vMax,  lr,lg,lb,
            fx, fy, fz,         r,g,b,  uMin, vMin,  lr,lg,lb,
            fx, fy+1.0f, fz+1.0f,  r,g,b,  uMax, vMax,  lr,lg,lb,
            fx, fy+1.0f, fz,    r,g,b,  uMin, vMax,  lr,lg,lb
        };
        vertices.insert(vertices.end(), faceData, faceData + 66);
    }
    // Right (x=1)
    else if(faceDir == 3)
    {
        float faceData[] = {
            fx+1.0f, fy, fz+1.0f,  r,g,b,  uMin, vMin,  lr,lg,lb,
            fx+1.0f, fy, fz,    r,g,b,  uMax, vMin,  lr,lg,lb,
            fx+1.0f, fy+1.0f, fz,  r,g,b,  uMax, vMax,  lr,lg,lb,
            fx+1.0f, fy, fz+1.0f,  r,g,b,  uMin, vMin,  lr,lg,lb,
            fx+1.0f, fy+1.0f, fz,  r,g,b,  uMax, vMax,  lr,lg,lb,
            fx+1.0f, fy+1.0f, fz+1.0f,r,g,b,  uMin, vMax,  lr,lg,lb
        };
        vertices.insert(vertices.end(), faceData, faceData + 66);
    }
    // Top (y=1)
    else if(faceDir == 4)
    {
        float faceData[] = {
            fx, fy+1.0f, fz+1.0f,  r,g,b,  uMin, vMin,  lr,lg,lb,
            fx+1.0f, fy+1.0f, fz+1.0f,r,g,b,  uMax, vMin,  lr,lg,lb,
            fx+1.0f, fy+1.0f, fz,  r,g,b,  uMax, vMax,  lr,lg,lb,
            fx, fy+1.0f, fz+1.0f,  r,g,b,  uMin, vMin,  lr,lg,lb,
            fx+1.0f, fy+1.0f, fz,  r,g,b,  uMax, vMax,  lr,lg,lb,
            fx, fy+1.0f, fz,    r,g,b,  uMin, vMax,  lr,lg,lb
        };
        vertices.insert(vertices.end(), faceData, faceData + 66);
    }
    // Bottom (y=0)
    else if(faceDir == 5)
    {
        float faceData[] = {
            fx, fy, fz,         r,g,b,  uMin, vMin,  lr,lg,lb,
            fx+1.0f, fy, fz,    r,g,b,  uMax, vMin,  lr,lg,lb,
            fx+1.0f, fy, fz+1.0f,  r,g,b,  uMax, vMax,  lr,lg,lb,
            fx, fy, fz,         r,g,b,  uMin, vMin,  lr,lg,lb,
            fx+1.0f, fy, fz+1.0f,  r,g,b,  uMax, vMax,  lr,lg,lb,
            fx, fy, fz+1.0f,       r,g,b,  uMin, vMax,  lr,lg,lb
        };
        vertices.insert(vertices.end(), faceData, faceData + 66);
    }
    
    indexCount += 6;
}

void Chunk::render(Shader& shader)
{
    if(meshDirty)
    {
        updateMesh();
        meshDirty = false;
    }
    
    glm::mat4 model = glm::mat4(1.0f);
    model = glm::translate(model, glm::vec3(chunkPosition * CHUNK_SIZE));
    shader.setMat4("model", model);
    
    glBindVertexArray(VAO);
    glDrawArrays(GL_TRIANGLES, 0, vertices.size() / 11);
}

bool Chunk::raycast(glm::vec3 origin, glm::vec3 direction, float maxDist, glm::ivec3& outputPos, glm::ivec3& outputPrePos)
{
    float step = 0.05f;
    // Convert world origin to local chunk origin
    glm::vec3 localOrigin = origin - glm::vec3(chunkPosition * CHUNK_SIZE);
    glm::vec3 pos = localOrigin;
    glm::vec3 lastPos = pos;
    
    for(float d = 0.0f; d < maxDist; d += step)
    {
        pos += direction * step;
        
        int x = (int)floor(pos.x);
        int y = (int)floor(pos.y);
        int z = (int)floor(pos.z);
        
        // Check bounds
        if(x >= 0 && x < CHUNK_SIZE && y >= 0 && y < CHUNK_SIZE && z >= 0 && z < CHUNK_SIZE)
        {
            if(blocks[x][y][z].isActive())
            {
                outputPos = glm::ivec3(x, y, z);
                outputPrePos = glm::ivec3((int)floor(lastPos.x), (int)floor(lastPos.y), (int)floor(lastPos.z));
                return true;
            }
        }
        
        lastPos = pos;
    }
    return false;
}

