#include "Chunk.h"
#include "World.h"
#include "WorldGenerator.h"
#include <glm/gtc/matrix_transform.hpp>
#include <queue>
#include <tuple>

Chunk::Chunk() : meshDirty(true), vertexCount(0), chunkPosition(0,0,0), world(nullptr), VAO(0), VBO(0), EBO(0)
{
    // GL initialization deferred to Main Thread via initGL()
    // Initialize with air
    for(int x=0; x<CHUNK_SIZE; ++x)
        for(int y=0; y<CHUNK_SIZE; ++y)
            for(int z=0; z<CHUNK_SIZE; ++z)
                blocks[x][y][z].type = AIR;
    
    for(int i=0; i<6; ++i) neighbors[i] = nullptr;
}

Chunk::~Chunk()
{
    glDeleteVertexArrays(1, &VAO);
    glDeleteBuffers(1, &VBO);
    glDeleteBuffers(1, &EBO);
}

// ... Setters ...

void Chunk::initGL() {
    if(VAO == 0) {
        glGenVertexArrays(1, &VAO);
        glGenBuffers(1, &VBO);
        glGenBuffers(1, &EBO);
    }
}

void Chunk::render(Shader& shader, const glm::mat4& viewProjection)
{
    if(vertexCount == 0) return;
    if(VAO == 0) initGL();
    
    shader.setMat4("model", glm::translate(glm::mat4(1.0f), glm::vec3(chunkPosition.x * CHUNK_SIZE, chunkPosition.y * CHUNK_SIZE, chunkPosition.z * CHUNK_SIZE)));
    
    glBindVertexArray(VAO);
    glDrawArrays(GL_TRIANGLES, 0, vertexCount);
    glBindVertexArray(0);
}

Block Chunk::getBlock(int x, int y, int z) const
{
    if(x < 0 || x >= CHUNK_SIZE || y < 0 || y >= CHUNK_SIZE || z < 0 || z >= CHUNK_SIZE)
        return {AIR};
    return blocks[x][y][z];
}

void Chunk::setBlock(int x, int y, int z, BlockType type)
{
    std::lock_guard<std::mutex> lock(chunkMutex);
    if(x < 0 || x >= CHUNK_SIZE || y < 0 || y >= CHUNK_SIZE || z < 0 || z >= CHUNK_SIZE)
        return;
    blocks[x][y][z].type = type;
    meshDirty = true;
}

uint8_t Chunk::getSkyLight(int x, int y, int z) const
{
    // Technically should lock, but single-byte read might be atomic-ish enough for visual artifacts...
    // But strictly, we should lock. However, locking on every get is EXPENSIVE.
    // For now, let's lock setters. get might read torn data but usually just old or new byte.
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
    std::lock_guard<std::mutex> lock(chunkMutex);
    if(x < 0 || x >= CHUNK_SIZE || y < 0 || y >= CHUNK_SIZE || z < 0 || z >= CHUNK_SIZE)
        return;
    blocks[x][y][z].skyLight = val;
    meshDirty = true;
}

void Chunk::setBlockLight(int x, int y, int z, uint8_t val)
{
    std::lock_guard<std::mutex> lock(chunkMutex);
    if(x < 0 || x >= CHUNK_SIZE || y < 0 || y >= CHUNK_SIZE || z < 0 || z >= CHUNK_SIZE)
        return;
    blocks[x][y][z].blockLight = val;
    meshDirty = true;
}

std::vector<float> Chunk::generateGeometry()
{
    // Thread safety: Lock while reading blocks
    std::lock_guard<std::mutex> lock(chunkMutex);
    
    std::vector<float> vertices;
    // Pre-allocate decent amount
    vertices.reserve(4096);

    // Greedy Meshing
    struct MaskInfo {
        BlockType type;
        uint8_t sky;
        uint8_t block;
        uint8_t ao[4]; // BL, BR, TR, TL
        
        bool operator==(const MaskInfo& other) const {
            return type == other.type && sky == other.sky && block == other.block &&
                   ao[0] == other.ao[0] && ao[1] == other.ao[1] && 
                   ao[2] == other.ao[2] && ao[3] == other.ao[3];
        }
        bool operator!=(const MaskInfo& other) const {
            return !(*this == other);
        }
    };
    
    // normal axis: 0=Z, 1=Z, 2=X, 3=X, 4=Y, 5=Y -> axis index: 2, 2, 0, 0, 1, 1

    for(int faceDir = 0; faceDir < 6; ++faceDir) {
        int axis = (faceDir <= 1) ? 2 : ((faceDir <= 3) ? 0 : 1);
        int uAxis = (axis == 0) ? 2 : ((axis == 1) ? 0 : 0);
        int vAxis = (axis == 0) ? 1 : ((axis == 1) ? 2 : 1);
        
        int nX=0, nY=0, nZ=0;
            if(faceDir==0) nZ=1; else if(faceDir==1) nZ=-1;
            else if(faceDir==2) nX=-1; else if(faceDir==3) nX=1;
            else if(faceDir==4) nY=1; else if(faceDir==5) nY=-1;

        auto getAt = [&](int u, int v, int d) -> Block {
                int p[3]; p[axis] = d; p[uAxis] = u; p[vAxis] = v;
                return blocks[p[0]][p[1]][p[2]];
        };
        auto getPos = [&](int u, int v, int d, int& ox, int& oy, int& oz) {
                int p[3]; p[axis] = d; p[uAxis] = u; p[vAxis] = v;
                ox = p[0]; oy = p[1]; oz = p[2];
        };

        for(int d = 0; d < CHUNK_SIZE; ++d) {
                MaskInfo mask[CHUNK_SIZE][CHUNK_SIZE];
                for(int u=0;u<CHUNK_SIZE;++u) for(int v=0;v<CHUNK_SIZE;++v) mask[u][v] = {AIR, 0, 0, {0,0,0,0}};

                for(int v=0; v<CHUNK_SIZE; ++v) {
                    for(int u=0; u<CHUNK_SIZE; ++u) {
                        Block b = getAt(u, v, d);
                        if(b.isActive()) {
                            int lx, ly, lz; getPos(u, v, d, lx, ly, lz);
                            int nx = lx + nX; int ny = ly + nY; int nz = lz + nZ;
                            
                            bool occluded = false;
                            uint8_t skyVal = 0; uint8_t blockVal = 0;

                            if(nx >= 0 && nx < CHUNK_SIZE && ny >= 0 && ny < CHUNK_SIZE && nz >= 0 && nz < CHUNK_SIZE) {
                                if(blocks[nx][ny][nz].isActive()) occluded = true;
                                else { skyVal = blocks[nx][ny][nz].skyLight; blockVal = blocks[nx][ny][nz].blockLight; }
                            } else {
                                // Use cached neighbors
                                int ni = -1;
                                int nnx=nx, nny=ny, nnz=nz;
                                
                                if(nz >= CHUNK_SIZE) { ni = DIR_FRONT; nnz -= CHUNK_SIZE; }
                                else if(nz < 0) { ni = DIR_BACK; nnz += CHUNK_SIZE; }
                                else if(nx < 0) { ni = DIR_LEFT; nnx += CHUNK_SIZE; }
                                else if(nx >= CHUNK_SIZE) { ni = DIR_RIGHT; nnx -= CHUNK_SIZE; }
                                else if(ny >= CHUNK_SIZE) { ni = DIR_TOP; nny -= CHUNK_SIZE; }
                                else if(ny < 0) { ni = DIR_BOTTOM; nny += CHUNK_SIZE; }
                                
                                if(ni != -1 && neighbors[ni]) {
                                     Block nb = neighbors[ni]->getBlock(nnx, nny, nnz);
                                     if(nb.isActive()) occluded = true;
                                     else { skyVal = neighbors[ni]->getSkyLight(nnx, nny, nnz); blockVal = neighbors[ni]->getBlockLight(nnx, nny, nnz); }
                                } else if(world) {
                                    // Fallback (Should rarely happen if neighbors are linked correctly)
                                    int gx = chunkPosition.x * CHUNK_SIZE + nx;
                                    int gy = chunkPosition.y * CHUNK_SIZE + ny;
                                    int gz = chunkPosition.z * CHUNK_SIZE + nz;
                                    Block nb = world->getBlock(gx, gy, gz);
                                    if(nb.isActive()) occluded = true;
                                    else { skyVal = world->getSkyLight(gx, gy, gz); blockVal = world->getBlockLight(gx, gy, gz); }
                                }
                            }
                            
                            if(!occluded) {
                                auto sampleAO = [&](int u1, int v1, int u2, int v2, int u3, int v3) -> uint8_t {
                                    auto check = [&](int u, int v) -> bool {
                                        int lx, ly, lz; getPos(u, v, d, lx, ly, lz);
                                        int nx=lx+nX; int ny=ly+nY; int nz=lz+nZ;
                                        if(nx >= 0 && nx < CHUNK_SIZE && ny >= 0 && ny < CHUNK_SIZE && nz >= 0 && nz < CHUNK_SIZE) 
                                            return blocks[nx][ny][nz].isOpaque();
                                            
                                        // Use Cached Neighbors
                                        int ni = -1;
                                        int nnx=nx, nny=ny, nnz=nz;
                                        if(nz >= CHUNK_SIZE) { ni = DIR_FRONT; nnz -= CHUNK_SIZE; }
                                        else if(nz < 0) { ni = DIR_BACK; nnz += CHUNK_SIZE; }
                                        else if(nx < 0) { ni = DIR_LEFT; nnx += CHUNK_SIZE; }
                                        else if(nx >= CHUNK_SIZE) { ni = DIR_RIGHT; nnx -= CHUNK_SIZE; }
                                        else if(ny >= CHUNK_SIZE) { ni = DIR_TOP; nny -= CHUNK_SIZE; }
                                        else if(ny < 0) { ni = DIR_BOTTOM; nny += CHUNK_SIZE; }
                                        
                                        if(ni != -1 && neighbors[ni]) {
                                            return neighbors[ni]->getBlock(nnx, nny, nnz).isOpaque();
                                        }

                                        if(world) {
                                            int gx = chunkPosition.x * CHUNK_SIZE + nx;
                                            int gy = chunkPosition.y * CHUNK_SIZE + ny;
                                            int gz = chunkPosition.z * CHUNK_SIZE + nz;
                                            return world->getBlock(gx, gy, gz).isOpaque();
                                        }
                                        return false;
                                    };
                                    bool s1 = check(u1, v1); bool s2 = check(u2, v2); bool c = check(u3, v3);
                                    if(s1 && s2) return 3;
                                    return (s1?1:0) + (s2?1:0) + (c?1:0);
                                };
                                uint8_t aos[4];
                                aos[0] = sampleAO(u-1, v, u, v-1, u-1, v-1); 
                                aos[1] = sampleAO(u+1, v, u, v-1, u+1, v-1); 
                                aos[2] = sampleAO(u+1, v, u, v+1, u+1, v+1); 
                                aos[3] = sampleAO(u-1, v, u, v+1, u-1, v+1); 
                                mask[u][v] = { (BlockType)b.type, skyVal, blockVal, {aos[0], aos[1], aos[2], aos[3]} };
                            }
                        }
                        }
                    }
                
                // Greedy Mesh
                for(int v=0; v<CHUNK_SIZE; ++v) {
                    for(int u=0; u<CHUNK_SIZE; ++u) {
                        if(mask[u][v].type != AIR) {
                            MaskInfo current = mask[u][v];
                            int w = 1, h = 1;
                            while(u + w < CHUNK_SIZE && mask[u+w][v] == current) w++;
                            bool canExtend = true;
                            while(v + h < CHUNK_SIZE && canExtend) {
                                for(int k=0; k<w; ++k) if(mask[u+k][v+h] != current) { canExtend = false; break; }
                                if(canExtend) h++;
                            }
                            int lx, ly, lz; getPos(u, v, d, lx, ly, lz);
                            addFace(vertices, lx, ly, lz, faceDir, current.type, w, h, current.ao[0], current.ao[1], current.ao[2], current.ao[3]);
                            for(int j=0; j<h; ++j) for(int i=0; i<w; ++i) mask[u+i][v+j] = {AIR, 0, 0, {0,0,0,0}};
                            u += w - 1; 
                        }
                    }
                }
        } // End d loop
    } // End faceDir loop
    return vertices;
}
void Chunk::uploadMesh(const std::vector<float>& data)
{
    if(VAO == 0) initGL();

    
    // Upload to GPU (Main Thread)
    glBindVertexArray(VAO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, data.size() * sizeof(float), data.data(), GL_STATIC_DRAW);
    
    vertexCount = data.size() / 13; // 13 floats per vert
    
    // Attribs
    float stride = 13 * sizeof(float);
    
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, (void*)0); // Pos
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, stride, (void*)(3 * sizeof(float))); // Color
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, stride, (void*)(6 * sizeof(float))); // UV
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(3, 3, GL_FLOAT, GL_FALSE, stride, (void*)(8 * sizeof(float))); // Light(Sky,Block,AO)
    glEnableVertexAttribArray(3);
    glVertexAttribPointer(4, 2, GL_FLOAT, GL_FALSE, stride, (void*)(11 * sizeof(float))); // TexOrigin
    glEnableVertexAttribArray(4);
}

void Chunk::updateMesh()
{
    std::vector<float> newVertices = generateGeometry();
    uploadMesh(newVertices);
    meshDirty = false;
}

void Chunk::addFace(std::vector<float>& vertices, int x, int y, int z, int faceDir, int blockType, int width, int height, int aoBL, int aoBR, int aoTR, int aoTL)
{
    float r, g, b;
    if (blockType == GRASS) { r = 0.0f; g = 1.0f; b = 0.0f; } 
    else if (blockType == DIRT) { r = 0.6f; g = 0.4f; b = 0.2f; }
    else if (blockType == STONE) { r = 1.0f; g = 1.0f; b = 1.0f; } 
    else if (blockType == WOOD) { r = 1.0f; g = 1.0f; b = 1.0f; } 
    else if (blockType == LEAVES) { r = 0.2f; g = 0.8f; b = 0.2f; } 
    else if (blockType == COAL_ORE || blockType == IRON_ORE || blockType == GLOWSTONE) { r = 1.0f; g = 1.0f; b = 1.0f; }
    else { r = 1.0f; g = 0.0f; b = 1.0f; }

    float l1 = 1.0f, l2 = 1.0f;
    float faceShade = 1.0f;
    if(faceDir == 4) faceShade = 1.0f;
    else if(faceDir == 5) faceShade = 0.6f;
    else faceShade = 0.8f;
    r *= faceShade; g *= faceShade; b *= faceShade;
    
    if(world) {
         int gx = chunkPosition.x * CHUNK_SIZE + x;
         int gy = chunkPosition.y * CHUNK_SIZE + y;
         int gz = chunkPosition.z * CHUNK_SIZE + z;
         int dx=0, dy=0, dz=0;
         if(faceDir==0) dz=1; else if(faceDir==1) dz=-1;
         else if(faceDir==2) dx=-1; else if(faceDir==3) dx=1;
         else if(faceDir==4) dy=1; else dy=-1;
         
         uint8_t s = world->getSkyLight(gx+dx, gy+dy, gz+dz);
         uint8_t bl = world->getBlockLight(gx+dx, gy+dy, gz+dz);
         l1 = pow((float)s/15.0f, 0.8f);
         l2 = pow((float)bl/15.0f, 0.8f);
    }

    float uMin = 0.00f, vMin = 0.00f; 
    
    if(blockType == DIRT) { uMin = 0.25f; vMin = 0.00f; }
    else if(blockType == GRASS) { uMin = 0.50f; vMin = 0.00f; }
    else if(blockType == WOOD) {
        if(faceDir == 4 || faceDir == 5) { uMin = 0.25f; vMin = 0.25f; } 
        else { uMin = 0.00f; vMin = 0.25f; } 
    } 
    else if(blockType == LEAVES) { uMin = 0.50f; vMin = 0.25f; }
    else if(blockType == COAL_ORE) { uMin = 0.75f; vMin = 0.00f; }
    else if(blockType == IRON_ORE) { uMin = 0.75f; vMin = 0.25f; }
    else if(blockType == GLOWSTONE) { uMin = 0.00f; vMin = 0.50f; }

    float fx = (float)x, fy = (float)y, fz = (float)z;
    float fw = (float)width, fh = (float)height;

    auto pushVert = [&](float vx, float vy, float vz, float u, float v, float ao) {
        vertices.push_back(vx); vertices.push_back(vy); vertices.push_back(vz);
        vertices.push_back(r); vertices.push_back(g); vertices.push_back(b);
        vertices.push_back(u); vertices.push_back(v); 
        vertices.push_back(l1); vertices.push_back(l2); vertices.push_back(ao);
        vertices.push_back(uMin); vertices.push_back(vMin); 
    };

    if(faceDir == 0) { // Front Z+
        // BL(0), BR(1), TR(2)
        pushVert(fx, fy, fz+1, 0, 0, (float)aoBL);
        pushVert(fx+fw, fy, fz+1, fw, 0, (float)aoBR);
        pushVert(fx+fw, fy+fh, fz+1, fw, fh, (float)aoTR);
        
        // BL(0), TR(2), TL(3)
        pushVert(fx, fy, fz+1, 0, 0, (float)aoBL);
        pushVert(fx+fw, fy+fh, fz+1, fw, fh, (float)aoTR);
        pushVert(fx, fy+fh, fz+1, 0, fh, (float)aoTL);
    }
    else if(faceDir == 1) { // Back Z-
        pushVert(fx+fw, fy, fz, 0, 0, (float)aoBR);
        pushVert(fx, fy, fz, fw, 0, (float)aoBL);
        pushVert(fx, fy+fh, fz, fw, fh, (float)aoTL);
        
        pushVert(fx+fw, fy, fz, 0, 0, (float)aoBR);
        pushVert(fx, fy+fh, fz, fw, fh, (float)aoTL);
        pushVert(fx+fw, fy+fh, fz, 0, fh, (float)aoTR);
    }
    else if(faceDir == 2) { // Left X-
        pushVert(fx, fy, fz, 0, 0, (float)aoBL);
        pushVert(fx, fy, fz+fw, fw, 0, (float)aoBR);
        pushVert(fx, fy+fh, fz+fw, fw, fh, (float)aoTR);
        
        pushVert(fx, fy, fz, 0, 0, (float)aoBL);
        pushVert(fx, fy+fh, fz+fw, fw, fh, (float)aoTR);
        pushVert(fx, fy+fh, fz, 0, fh, (float)aoTL);
    }
    else if(faceDir == 3) { // Right X+
        pushVert(fx+1, fy, fz+fw, 0, 0, (float)aoBR);
        pushVert(fx+1, fy, fz, fw, 0, (float)aoBL);
        pushVert(fx+1, fy+fh, fz, fw, fh, (float)aoTL);
        
        pushVert(fx+1, fy, fz+fw, 0, 0, (float)aoBR);
        pushVert(fx+1, fy+fh, fz, fw, fh, (float)aoTL); 
        pushVert(fx+1, fy+fh, fz+fw, 0, fh, (float)aoTR);
    }
    else if(faceDir == 4) { // Top Y+
        // Reverting Z-swap. Restoring Logic:
        // 1. (x, z+h)   -> (minX, maxZ) -> Top-Left -> aoTL (valAO[3])
        // 2. (x+w, z+h) -> (maxX, maxZ) -> Top-Right -> aoTR (valAO[2])
        // 3. (x+w, z)   -> (maxX, minZ) -> Bottom-Right -> aoBR (valAO[1])
        // 4. (x, z)     -> (minX, minZ) -> Bottom-Left -> aoBL (valAO[0])
        
        // Quad 1: TL, TR, BR
        pushVert(fx, fy+1, fz+fh, 0, 0, (float)aoTL); 
        pushVert(fx+fw, fy+1, fz+fh, fw, 0, (float)aoTR);
        pushVert(fx+fw, fy+1, fz, fw, fh, (float)aoBR);
        
        // Quad 2: TL, BR, BL
        pushVert(fx, fy+1, fz+fh, 0, 0, (float)aoTL);
        pushVert(fx+fw, fy+1, fz, fw, fh, (float)aoBR);
        pushVert(fx, fy+1, fz, 0, fh, (float)aoBL);
    }
    else { // Bottom Y-
        // Reverting Z-swap
        // 1. (x, z)     -> (minX, minZ) -> aoBL
        // 2. (x+w, z)   -> (maxX, minZ) -> aoBR
        // 3. (x+w, z+h) -> (maxX, maxZ) -> aoTR
        // 4. (x, z+h)   -> (minX, maxZ) -> aoTL
        
        pushVert(fx, fy, fz, 0, 0, (float)aoBL);
        pushVert(fx+fw, fy, fz, fw, 0, (float)aoBR);
        pushVert(fx+fw, fy, fz+fh, fw, fh, (float)aoTR);
        
        pushVert(fx, fy, fz, 0, 0, (float)aoBL);
        pushVert(fx+fw, fy, fz+fh, fw, fh, (float)aoTR);
        pushVert(fx, fy, fz+fh, 0, fh, (float)aoTL);
    }
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
    std::lock_guard<std::mutex> lock(chunkMutex);
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
             
             // Optimization: Use neighbors[DIR_TOP] to walk up
             Chunk* current = this;
             int currentY = chunkPosition.y;
             
             // Walk up using neighbors
             while(current->neighbors[DIR_TOP]) {
                 current = current->neighbors[DIR_TOP];
                 currentY++;
                 // Check column in upper chunk
                 bool blocked = false;
                 for(int ly=0; ly<CHUNK_SIZE; ++ly) {
                     if(current->getBlock(x, ly, z).isActive()) {
                         exposedToSky = false;
                         blocked = true;
                         break;
                     }
                 }
                 if(blocked) break;
             }
             
             // If we ran out of neighbors but didn't hit world height limit?
             // Since we don't have infinite height, we assume things above the last known chunk are AIR?
             // Or we fallback to WorldGenerator/World lookup if neighbor link is missing but chunk exists
             
             if(exposedToSky && currentY < (127/CHUNK_SIZE)) {
                  // Fallback to World if we have holes in neighbor links (e.g. diagonal loading?)
                  // Or just WorldGenerator check
                  if(world) {
                       int gx = chunkPosition.x * CHUNK_SIZE + x;
                       int gz = chunkPosition.z * CHUNK_SIZE + z;
                       int terrainH = WorldGenerator::GetHeight(gx, gz);
                       if(chunkPosition.y * CHUNK_SIZE <= terrainH + 6) {
                            // Only assume shadow if we are deep enough.
                            // If we are high up, we are exposed (unless blocked by check above).
                            // But wait, if we are below terrain height, we should be shadowed?
                            // Yes, if we haven't found a blocking block yet, but we are essentially 'inside' the terrain 
                            // (which might not be generated yet above us?)
                            
                            // If chunks above are not generated, we must assume something.
                            // If we assume SKY, then when they generate as SOLID, we have to recalculate. (Harder)
                            // If we assume DARK, then when they generate as AIR, we have to recalculate. (Easy - they trigger update)
                            
                            // So if neighbor is missing, we check Heightmap.
                            // If Y < Heightmap, assume blocked.
                            int myMaxY = chunkPosition.y * CHUNK_SIZE + CHUNK_SIZE;
                            if(myMaxY <= terrainH) exposedToSky = false;
                       }
                  }
             }

             if(false) { // Disable old logic
                 if(world) {
                 int startCyIndex = chunkPosition.y + 1;
                 int endCyIndex = 127 / CHUNK_SIZE; // Max world height index
                 
                 for(int cy_idx = startCyIndex; cy_idx <= endCyIndex; ++cy_idx) {
                     int cx = chunkPosition.x;
                     int cz = chunkPosition.z;
                     
                     Chunk* c = world->getChunk(cx, cy_idx, cz);
                     if(c) {
                         // Check all blocks in this chunk column
                         bool blocked = false;
                         for(int ly=0; ly<CHUNK_SIZE; ++ly) {
                             if(c->getBlock(x, ly, z).isActive()) {
                                 exposedToSky = false;
                                 blocked = true;
                                 break;
                             }
                         }
                         if(blocked) break;
                     } else {
                         // Fallback using Heightmap
                         int chunkBaseY = cy_idx * CHUNK_SIZE;
                         int terrainH = WorldGenerator::GetHeight(gx, gz);
                         
                         // If the bottom of this chunk is below the potential terrain/tree height
                         if(chunkBaseY <= terrainH + 6) {
                             // Assuming solid if missing and below height
                             exposedToSky = false;
                             break;
                         }
                     }
                 }
             }
             } // End if(false)
             
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
    std::lock_guard<std::mutex> lock(chunkMutex);
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
    std::lock_guard<std::mutex> lock(chunkMutex);
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
    // Neighbors: Left(-X), Right(+X), Back(-Z), Front(+Z), Bottom(-Y), Top(+Y)
    struct NeighPtr { int ni; int ox, oy, oz; int faceAxis; };
    NeighPtr nPtrs[] = {
        {DIR_LEFT,  CHUNK_SIZE-1, 0, 0,   0},
        {DIR_RIGHT, 0, 0, 0,              0},
        {DIR_BACK,  0, 0, CHUNK_SIZE-1,   2}, // Back is Z- (Wait, in GreedyMesh faceDir=1 was Z- and called Back?)
                                              // Let's standardise:
                                              // Z- (Back) -> neighbors[DIR_BACK]
                                              // Z+ (Front) -> neighbors[DIR_FRONT]
                                              // X- (Left) -> neighbors[DIR_LEFT]
                                              // X+ (Right) -> neighbors[DIR_RIGHT]
                                              // Y- (Bottom) -> neighbors[DIR_BOTTOM]
                                              // Y+ (Top) -> neighbors[DIR_TOP]
        {DIR_FRONT, 0, 0, 0,              2},
        {DIR_BOTTOM,0, CHUNK_SIZE-1, 0,   1},
        {DIR_TOP,   0, 0, 0,              1}
    };
    // Note: nPtrs array matches iteration order or just explicit check?
    // The loop below iterates 'neighbors' array which was struct...
    // Let's rewrite the loop using explicit neighbors array
    
    for(const auto& np : nPtrs) {
        Chunk* nc = neighbors[np.ni];
        if(nc) {
             // Iterate face
             for(int u=0; u<CHUNK_SIZE; ++u) {
                 for(int v=0; v<CHUNK_SIZE; ++v) {
                       int lx, ly, lz; 
                       int nx, ny, nz; 
                       
                       // Define iteration based on Face Axis (0=X-face, 1=Y-face, 2=Z-face)
                       // If Axis=0 (Left/Right), u=y, v=z
                       // If Axis=1 (Bot/Top), u=x, v=z
                       // If Axis=2 (Back/Front), u=x, v=y
                       
                       if(np.faceAxis == 0) { // X neighbors
                            lx = (np.ni == DIR_LEFT) ? 0 : CHUNK_SIZE-1;
                            ly = u; lz = v;
                            nx = np.ox; ny = u; nz = v;
                       } else if(np.faceAxis == 1) { // Y neighbors
                            lx = u; ly = (np.ni == DIR_BOTTOM) ? 0 : CHUNK_SIZE-1; lz = v;
                            nx = u; ny = np.oy; nz = v; // np.oy is boundary
                       } else { // Z neighbors
                            lx = u; ly = v; lz = (np.ni == DIR_BACK) ? 0 : CHUNK_SIZE-1;
                            nx = u; ny = v; nz = np.oz;
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

    if(false) { // Disable old logic
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
    } // End if(false)

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
            } else {
                // Propagate to neighbors?
                // Now that we have cached neighbors, we can potentially push to their queues?
                // Or just updating self and letting them pull?
                // Standard MC lighting often pushes to neighbors.
                // But thread safety? Calling flavor text on neighbor?
                // Neighbors might be meshing. setSkyLight locks chunkMutex.
                // So calling neighbor->setSkyLight() IS thread safe (Chunk mutex).
                
                int ni = -1;
                int nnx=nx, nny=ny, nnz=nz;
                if(nz >= CHUNK_SIZE) { ni = DIR_FRONT; nnz -= CHUNK_SIZE; }
                else if(nz < 0) { ni = DIR_BACK; nnz += CHUNK_SIZE; }
                else if(nx < 0) { ni = DIR_LEFT; nnx += CHUNK_SIZE; }
                else if(nx >= CHUNK_SIZE) { ni = DIR_RIGHT; nnx -= CHUNK_SIZE; }
                else if(ny >= CHUNK_SIZE) { ni = DIR_TOP; nny -= CHUNK_SIZE; }
                else if(ny < 0) { ni = DIR_BOTTOM; nny += CHUNK_SIZE; }
                
                if(ni != -1 && neighbors[ni]) {
                     Chunk* nc = neighbors[ni];
                     // Lock neighbor to read/write light
                     // Note: Deadlock risk if A locks B while B locks A?
                     // spreadLight is usually called from World::Update or Worker.
                     // If two threads process adjacent chunks and try to spread to each other...
                     // A holds A.mutex, Tries B.mutex.
                     // B holds B.mutex, Tries A.mutex.
                     // DEADLOCK.
                     
                     // Solution: Do NOT propagate to neighbors here directly if it locks.
                     // Or use std::try_lock?
                     // Or just mark neighbor as dirty or queue an update for it?
                     
                     // Current architecture relies on pulling from neighbors (Seed step).
                     // But BFS needs to push. 
                     
                     // For now, let's Stick to LOCAL chunk spreading.
                     // Cross-chunk propagation is handled by World::setBlock Triggering neighbor updates.
                     // AND implementation of "Pass 2" in main which calls spreadLight on everyone.
                     
                     // However, real propagation across boundaries requires loop.
                     // If we just stop at boundary, light stops.
                     // But we have the Pull-System in Step 2 of spreadLight. 
                     // The neighbor will see OUR high light and pull it in during ITS spreadLight.
                     // So as long as we iterate spreadLight multiple times or in order...
                     
                     // So we don't need to push to neighbors here?
                     // Ideally yes.
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

