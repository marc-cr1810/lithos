#include "Chunk.h"
#include "World.h"
#include "WorldGenerator.h"
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

uint8_t Chunk::getSkyLight(int x, int y, int z) const
{
    if(x < 0 || x >= CHUNK_SIZE || y < 0 || y >= CHUNK_SIZE || z < 0 || z >= CHUNK_SIZE)
        return 0;
    return blocks[x][y][z].skyLight;
}

uint8_t Chunk::getBlockLight(int x, int y, int z) const
{
    if(x < 0 || x >= CHUNK_SIZE || y < 0 || y >= CHUNK_SIZE || z < 0 || z >= CHUNK_SIZE)
        return 0;
    return blocks[x][y][z].blockLight;
}

void Chunk::setSkyLight(int x, int y, int z, uint8_t val)
{
    if(x < 0 || x >= CHUNK_SIZE || y < 0 || y >= CHUNK_SIZE || z < 0 || z >= CHUNK_SIZE)
        return;
    blocks[x][y][z].skyLight = val;
}

void Chunk::setBlockLight(int x, int y, int z, uint8_t val)
{
    if(x < 0 || x >= CHUNK_SIZE || y < 0 || y >= CHUNK_SIZE || z < 0 || z >= CHUNK_SIZE)
        return;
    blocks[x][y][z].blockLight = val;
}

void Chunk::updateMesh()
{
    vertices.clear();
    indices.clear();
    indexCount = 0;
    
    unsigned int vIndex = 0;
    
    // -------------------------------------------------------
    // Lighting Calculation (BFS) - MOVED TO calculateSunlight/spreadLight
    // -------------------------------------------------------
    
    // We assume light is already calculated when updateMesh runs.
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
    // Ores: White Tint (Use texture)
    else if (blockType == COAL_ORE || blockType == IRON_ORE || blockType == GLOWSTONE) { r = 1.0f; g = 1.0f; b = 1.0f; }
    else { r = 1.0f; g = 0.0f; b = 1.0f; } // Pink error

    // Sky/Block Light
    // Get light from the air block next to the face
    float skyVal = 1.0f; // Default 15? No default 0 if dark?
    float blockVal = 0.0f;
    int nx=0, ny=0, nz=0;
    
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

        int nx_local = gx+dx; int ny_local = gy+dy; int nz_local = gz+dz;
        nx = nx_local; ny = ny_local; nz = nz_local;
        
        uint8_t s = world->getSkyLight(nx, ny, nz);
        uint8_t bl = world->getBlockLight(nx, ny, nz);
        
        // Convert to float 0-1
        skyVal = (float)s / 15.0f;
        blockVal = (float)bl / 15.0f;
        
        // Gamma/Curve
        skyVal = pow(skyVal, 0.8f); // Tweak power
        blockVal = pow(blockVal, 0.8f);
    }

    // Face Shading (Ambient Occlusion placeholder / Directional Shade)
    float faceShade = 1.0f;
    if(faceDir == 4) faceShade = 1.0f;        // Top
    else if(faceDir == 5) faceShade = 0.6f;   // Bottom
    else faceShade = 0.8f;                    // Side
    
    // Apply Face Shade to Color Tint
    r *= faceShade;
    g *= faceShade;
    b *= faceShade;

    // Light Attribute now passes: Sky, Block, AO
    // AO Calculation
    // We need to calculate AO for 4 corners of the face.
    // Neighbors relative to the AIR BLOCK (nx, ny, nz).
    
    float aoBL=0, aoBR=0, aoTL=0, aoTR=0; // 0..3
    
    if(world) {
        auto isOcc = [&](int dX, int dY, int dZ) {
            return world->getBlock(nx+dX, ny+dY, nz+dZ).isActive();
        };

        bool t=false, b=false, l=false, r=false;
        bool tl=false, tr=false, bl=false, br=false;
        
        // Define T, B, L, R relative to face
        if(faceDir == 0 || faceDir == 1) { // Front/Back (Z axis)
             // Up=Y+, Right=X+ (for Front)
             // Actually, Right depends on winding, but let's stick to world axis.
             t = isOcc(0, 1, 0);
             b = isOcc(0, -1, 0);
             l = isOcc(-1, 0, 0);
             r = isOcc(1, 0, 0);
             tl = isOcc(-1, 1, 0);
             tr = isOcc(1, 1, 0);
             bl = isOcc(-1, -1, 0);
             br = isOcc(1, -1, 0);
             
             // Map to corners (assuming Standard quad orientation)
             // BL = (-1, -1), BR = (1, -1), TR = (1, 1), TL = (-1, 1)
             aoBL = vertexAO(l, b, bl);
             aoBR = vertexAO(r, b, br);
             aoTR = vertexAO(r, t, tr);
             aoTL = vertexAO(l, t, tl);
        }
        else if(faceDir == 2 || faceDir == 3) { // Lefty/Right (X axis)
             // Up=Y+, Right=Z+ (For Right?)
             t = isOcc(0, 1, 0);
             b = isOcc(0, -1, 0);
             l = isOcc(0, 0, -1); // Z-
             r = isOcc(0, 0, 1);  // Z+
             tl = isOcc(0, 1, -1);
             tr = isOcc(0, 1, 1);
             bl = isOcc(0, -1, -1);
             br = isOcc(0, -1, 1);
             
             // BL (Z-, Y-), BR (Z+, Y-), TR (Z+, Y+), TL (Z-, Y+)
             aoBL = vertexAO(l, b, bl);
             aoBR = vertexAO(r, b, br);
             aoTR = vertexAO(r, t, tr);
             aoTL = vertexAO(l, t, tl);
        }
        else { // Top/Bottom (Y axis)
             // Up=Z+, Right=X+
             t = isOcc(0, 0, 1); // Z+ (Top in 2D plane)
             b = isOcc(0, 0, -1); // Z-
             l = isOcc(-1, 0, 0); // X-
             r = isOcc(1, 0, 0); // X+
             tl = isOcc(-1, 0, 1);
             tr = isOcc(1, 0, 1);
             bl = isOcc(-1, 0, -1);
             br = isOcc(1, 0, -1);
             
             // BL (X-, Z-), BR (X+, Z-), TR (X+, Z+), TL (X-, Z+)
             aoBL = vertexAO(l, b, bl);
             aoBR = vertexAO(r, b, br);
             aoTR = vertexAO(r, t, tr);
             aoTL = vertexAO(l, t, tl);
        }
    }

    float l1 = skyVal;
    float l2 = blockVal;
    // l3 is now unused placeholder in array setup, we will replace usage below.
    float l3 = 0.0f;


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
    else if(blockType == GLOWSTONE) {
        // Glowstone (0,2)
        uMin = 0.00f; uMax = 0.25f;
        vMin = 0.50f; vMax = 0.75f;
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
            fx, fy, fz+1.0f,    r,g,b,  uMin, vMin,  l1,l2,aoBL,
            fx+1.0f, fy, fz+1.0f,  r,g,b,  uMax, vMin,  l1,l2,aoBR,
            fx+1.0f, fy+1.0f, fz+1.0f,r,g,b,  uMax, vMax,  l1,l2,aoTR,
            fx, fy, fz+1.0f,    r,g,b,  uMin, vMin,  l1,l2,aoBL,
            fx+1.0f, fy+1.0f, fz+1.0f,r,g,b,  uMax, vMax,  l1,l2,aoTR,
            fx, fy+1.0f, fz+1.0f,  r,g,b,  uMin, vMax,  l1,l2,aoTL
        };
        vertices.insert(vertices.end(), faceData, faceData + 66);
    }
    // Back face (z=0)
    else if(faceDir == 1) // Back
    {
        float faceData[] = {
            fx+1.0f, fy, fz,    r,g,b,  uMin, vMin,  l1,l2,aoBR,
            fx, fy, fz,         r,g,b,  uMax, vMin,  l1,l2,aoBL,
            fx, fy+1.0f, fz,    r,g,b,  uMax, vMax,  l1,l2,aoTL,
            fx+1.0f, fy, fz,    r,g,b,  uMin, vMin,  l1,l2,aoBR,
            fx, fy+1.0f, fz,    r,g,b,  uMax, vMax,  l1,l2,aoTL,
            fx+1.0f, fy+1.0f, fz,  r,g,b,  uMin, vMax,  l1,l2,aoTR
        };
        vertices.insert(vertices.end(), faceData, faceData + 66);
    }
    // Left (x=0)
    else if(faceDir == 2)
    {
        float faceData[] = {
            fx, fy, fz,         r,g,b,  uMin, vMin,  l1,l2,aoBL,
            fx, fy, fz+1.0f,    r,g,b,  uMax, vMin,  l1,l2,aoBR,
            fx, fy+1.0f, fz+1.0f,  r,g,b,  uMax, vMax,  l1,l2,aoTR,
            fx, fy, fz,         r,g,b,  uMin, vMin,  l1,l2,aoBL,
            fx, fy+1.0f, fz+1.0f,  r,g,b,  uMax, vMax,  l1,l2,aoTR,
            fx, fy+1.0f, fz,    r,g,b,  uMin, vMax,  l1,l2,aoTL
        };
        vertices.insert(vertices.end(), faceData, faceData + 66);
    }
    // Right (x=1)
    else if(faceDir == 3)
    {
         float faceData[] = {
            fx+1.0f, fy, fz+1.0f,    r,g,b,  uMin, vMin,  l1,l2,aoBR,
            fx+1.0f, fy, fz,         r,g,b,  uMax, vMin,  l1,l2,aoBL,
            fx+1.0f, fy+1.0f, fz,    r,g,b,  uMax, vMax,  l1,l2,aoTL,
            fx+1.0f, fy, fz+1.0f,    r,g,b,  uMin, vMin,  l1,l2,aoBR,
            fx+1.0f, fy+1.0f, fz,    r,g,b,  uMax, vMax,  l1,l2,aoTL,
            fx+1.0f, fy+1.0f, fz+1.0f,  r,g,b,  uMin, vMax,  l1,l2,aoTR
        };
        vertices.insert(vertices.end(), faceData, faceData + 66);
    }
    // Top (y=1)
    else if(faceDir == 4)
    {
        float faceData[] = {
            fx, fy+1.0f, fz+1.0f,    r,g,b,  uMin, vMin,  l1,l2,aoTL,
            fx+1.0f, fy+1.0f, fz+1.0f,  r,g,b,  uMax, vMin,  l1,l2,aoTR,
            fx+1.0f, fy+1.0f, fz,    r,g,b,  uMax, vMax,  l1,l2,aoBR,
            fx, fy+1.0f, fz+1.0f,    r,g,b,  uMin, vMin,  l1,l2,aoTL,
            fx+1.0f, fy+1.0f, fz,    r,g,b,  uMax, vMax,  l1,l2,aoBR,
            fx, fy+1.0f, fz,      r,g,b,  uMin, vMax,  l1,l2,aoBL
        };
        vertices.insert(vertices.end(), faceData, faceData + 66);
    }
    // Bottom (y=0)
    else
    {
        float faceData[] = {
            fx, fy, fz,         r,g,b,  uMin, vMin,  l1,l2,aoBL,
            fx+1.0f, fy, fz,    r,g,b,  uMax, vMin,  l1,l2,aoBR,
            fx+1.0f, fy, fz+1.0f,  r,g,b,  uMax, vMax,  l1,l2,aoTR,
            fx, fy, fz,         r,g,b,  uMin, vMin,  l1,l2,aoBL,
            fx+1.0f, fy, fz+1.0f,  r,g,b,  uMax, vMax,  l1,l2,aoTR,
            fx, fy, fz+1.0f,    r,g,b,  uMin, vMax,  l1,l2,aoTL
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

void Chunk::calculateSunlight() {
    // 1. Reset Sky Light
    for(int x=0; x<CHUNK_SIZE; ++x)
        for(int y=0; y<CHUNK_SIZE; ++y)
            for(int z=0; z<CHUNK_SIZE; ++z)
                blocks[x][y][z].skyLight = 0;

    // 2. Sunlight Column Calculation (Y-Down)
    for(int x=0; x<CHUNK_SIZE; ++x) {
        for(int z=0; z<CHUNK_SIZE; ++z) {
             // 2a. Determine if column start has access to sky
             int gx = chunkPosition.x * CHUNK_SIZE + x;
             int gz = chunkPosition.z * CHUNK_SIZE + z;
             
             bool exposedToSky = true;
             if(world) {
                 int startGy = (chunkPosition.y + 1) * CHUNK_SIZE;
                 for(int cy = startGy; cy < 128; ++cy) { 
                     int cx = (int)floor((float)gx / CHUNK_SIZE);
                     int cz = (int)floor((float)gz / CHUNK_SIZE);
                     int cY_index = (int)floor((float)cy / CHUNK_SIZE);
                     
                     Chunk* c = world->getChunk(cx, cY_index, cz);
                     if(c) {
                         int lx = gx % CHUNK_SIZE; if(lx<0) lx+=CHUNK_SIZE;
                         int ly = cy % CHUNK_SIZE; if(ly<0) ly+=CHUNK_SIZE;
                         int lz = gz % CHUNK_SIZE; if(lz<0) lz+=CHUNK_SIZE;
                         if(c->getBlock(lx, ly, lz).isActive()) {
                             exposedToSky = false;
                             break;
                         }
                     } else {
                         int terrainH = WorldGenerator::GetHeight(gx, gz);
                         if(cy <= terrainH + 5) {
                             exposedToSky = false;
                             break;
                         }
                     }
                 }
             }
             
             if(exposedToSky) {
                 for(int y=CHUNK_SIZE-1; y>=0; --y) {
                     if(blocks[x][y][z].isActive()) {
                         break;
                     } else {
                         blocks[x][y][z].skyLight = 15;
                     }
                 }
             }
        }
    }
}

void Chunk::calculateBlockLight() {
    // 1. Reset and Seed Block Light
    for(int x=0; x<CHUNK_SIZE; ++x) {
        for(int y=0; y<CHUNK_SIZE; ++y) {
            for(int z=0; z<CHUNK_SIZE; ++z) {
                blocks[x][y][z].blockLight = 0;
                
                if(blocks[x][y][z].isActive()) {
                    uint8_t emission = blocks[x][y][z].getEmission();
                    if(emission > 0) {
                        blocks[x][y][z].blockLight = emission;
                    }
                }
            }
        }
    }
}

void Chunk::spreadLight() {
    std::queue<glm::ivec3> skyQueue;
    std::queue<glm::ivec3> blockQueue;
    
    // 1. Seed from self
    for(int x=0; x<CHUNK_SIZE; ++x) {
        for(int y=0; y<CHUNK_SIZE; ++y) {
            for(int z=0; z<CHUNK_SIZE; ++z) {
                if(blocks[x][y][z].skyLight > 1) {
                    skyQueue.push(glm::ivec3(x, y, z));
                }
                if(blocks[x][y][z].blockLight > 1) {
                    blockQueue.push(glm::ivec3(x, y, z));
                }
            }
        }
    }

    // 2. Seed from Neighbor Chunks
    if(world) {
        int cx = chunkPosition.x;
        int cy = chunkPosition.y;
        int cz = chunkPosition.z;

        struct Neighbor { int dx, dy, dz; int ox, oy, oz; int faceAxis; };
        Neighbor neighbors[] = {
            {-1, 0, 0,  CHUNK_SIZE-1, 0, 0,   0},
            { 1, 0, 0,  0, 0, 0,              0},
            { 0,-1, 0,  0, CHUNK_SIZE-1, 0,   1},
            { 0, 1, 0,  0, 0, 0,              1},
            { 0, 0,-1,  0, 0, CHUNK_SIZE-1,   2},
            { 0, 0, 1,  0, 0, 0,              2}
        };

        for(const auto& n : neighbors) {
            Chunk* nc = world->getChunk(cx + n.dx, cy + n.dy, cz + n.dz);
            if(nc) {
                for(int u=0; u<CHUNK_SIZE; ++u) {
                    for(int v=0; v<CHUNK_SIZE; ++v) {
                        int lx, ly, lz; 
                        int nx, ny, nz; 

                        if(n.faceAxis == 0) {
                            lx = (n.dx == -1) ? 0 : CHUNK_SIZE-1;
                            ly = u; lz = v;
                            nx = n.ox; ny = u; nz = v;
                        } else if(n.faceAxis == 1) {
                            lx = u; ly = (n.dy == -1) ? 0 : CHUNK_SIZE-1; lz = v;
                            nx = u; ny = n.oy; nz = v;
                        } else {
                            lx = u; ly = v; lz = (n.dz == -1) ? 0 : CHUNK_SIZE-1;
                            nx = u; ny = v; nz = n.oz;
                        }

                        if(!blocks[lx][ly][lz].isActive()) {
                             // Sky Light
                             uint8_t nSky = nc->getSkyLight(nx, ny, nz);
                             if(nSky > 1 && nSky - 1 > blocks[lx][ly][lz].skyLight) {
                                 blocks[lx][ly][lz].skyLight = nSky - 1;
                                 skyQueue.push(glm::ivec3(lx, ly, lz));
                                 meshDirty = true;
                             }
                             // Block Light
                             uint8_t nBlock = nc->getBlockLight(nx, ny, nz);
                             if(nBlock > 1 && nBlock - 1 > blocks[lx][ly][lz].blockLight) {
                                 blocks[lx][ly][lz].blockLight = nBlock - 1;
                                 blockQueue.push(glm::ivec3(lx, ly, lz));
                                 meshDirty = true;
                             }
                        }
                    }
                }
            }
        }
    }

    // 3. Propagate BFS
    int directions[6][3] = {
        {1,0,0}, {-1,0,0},
        {0,1,0}, {0,-1,0},
        {0,0,1}, {0,0,-1}
    };

    // Process Sky Light
    while(!skyQueue.empty()) {
        glm::ivec3 pos = skyQueue.front();
        skyQueue.pop();
        
        int curLight = blocks[pos.x][pos.y][pos.z].skyLight;
        if(curLight <= 1) continue; 

        for(int i=0; i<6; ++i) {
            int nx = pos.x + directions[i][0];
            int ny = pos.y + directions[i][1];
            int nz = pos.z + directions[i][2];

            if(nx >= 0 && nx < CHUNK_SIZE && ny >= 0 && ny < CHUNK_SIZE && nz >= 0 && nz < CHUNK_SIZE) {
                if(!blocks[nx][ny][nz].isActive()) {
                    if(blocks[nx][ny][nz].skyLight < curLight - 1) {
                        blocks[nx][ny][nz].skyLight = curLight - 1;
                        skyQueue.push(glm::ivec3(nx, ny, nz));
                        meshDirty = true;
                    }
                }
            }
        }
    }
    
    // Process Block Light
    while(!blockQueue.empty()) {
        glm::ivec3 pos = blockQueue.front();
        blockQueue.pop();
        
        int curLight = blocks[pos.x][pos.y][pos.z].blockLight;
        if(curLight <= 1) continue; 

        for(int i=0; i<6; ++i) {
            int nx = pos.x + directions[i][0];
            int ny = pos.y + directions[i][1];
            int nz = pos.z + directions[i][2];

            if(nx >= 0 && nx < CHUNK_SIZE && ny >= 0 && ny < CHUNK_SIZE && nz >= 0 && nz < CHUNK_SIZE) {
                if(!blocks[nx][ny][nz].isActive()) {
                    if(blocks[nx][ny][nz].blockLight < curLight - 1) {
                        blocks[nx][ny][nz].blockLight = curLight - 1;
                        blockQueue.push(glm::ivec3(nx, ny, nz));
                        meshDirty = true;
                    }
                }
            }
        }
    }
}

// Helper for Ambient Occlusion
// side1, side2 are the two blocks next to the vertex on the face plane
// corner is the block diagonally from the vertex
int Chunk::vertexAO(bool side1, bool side2, bool corner) {
    if(side1 && side2) {
        return 3;
    }
    return (side1 ? 1 : 0) + (side2 ? 1 : 0) + (corner ? 1 : 0);
}

