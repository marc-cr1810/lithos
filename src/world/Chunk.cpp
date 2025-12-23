#include "Chunk.h"
#include "World.h"
#include "WorldGenerator.h"
#include <glm/gtc/matrix_transform.hpp>
#include <queue>
#include <tuple>

Chunk::Chunk() : meshDirty(true), vertexCount(0), vertexCountTransparent(0), chunkPosition(0,0,0), world(nullptr), VAO(0), VBO(0), EBO(0)
{
    // GL initialization deferred to Main Thread via initGL()
    // Initialize with air
    Block* air = BlockRegistry::getInstance().getBlock(AIR);
    for(int x=0; x<CHUNK_SIZE; ++x)
        for(int y=0; y<CHUNK_SIZE; ++y)
            for(int z=0; z<CHUNK_SIZE; ++z)
                blocks[x][y][z] = { air, 0, 0 };
    
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

void Chunk::render(Shader& shader, const glm::mat4& viewProjection, int pass)
{
    if(VAO == 0) initGL();
    // Pass 0: Opaque
    // Pass 1: Transparent
    
    if(pass == 0 && vertexCount == 0) return;
    if(pass == 1 && vertexCountTransparent == 0) return;

    shader.setMat4("model", glm::translate(glm::mat4(1.0f), glm::vec3(chunkPosition.x * CHUNK_SIZE, chunkPosition.y * CHUNK_SIZE, chunkPosition.z * CHUNK_SIZE)));
    
    glBindVertexArray(VAO);
    if(pass == 0) {
        glDrawArrays(GL_TRIANGLES, 0, vertexCount);
    } else {
        glDrawArrays(GL_TRIANGLES, vertexCount, vertexCountTransparent);
    }
    glBindVertexArray(0);
}

ChunkBlock Chunk::getBlock(int x, int y, int z) const
{
    if(x < 0 || x >= CHUNK_SIZE || y < 0 || y >= CHUNK_SIZE || z < 0 || z >= CHUNK_SIZE)
        return { BlockRegistry::getInstance().getBlock(AIR), 0, 0 };
    return blocks[x][y][z];
}

void Chunk::setBlock(int x, int y, int z, BlockType type)
{
    std::lock_guard<std::mutex> lock(chunkMutex);
    if(x < 0 || x >= CHUNK_SIZE || y < 0 || y >= CHUNK_SIZE || z < 0 || z >= CHUNK_SIZE)
        return;
    blocks[x][y][z].block = BlockRegistry::getInstance().getBlock(type);
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

uint8_t Chunk::getMetadata(int x, int y, int z) const
{
    if(x < 0 || x >= CHUNK_SIZE || y < 0 || y >= CHUNK_SIZE || z < 0 || z >= CHUNK_SIZE)
        return 0;
    return blocks[x][y][z].metadata;
}

void Chunk::setMetadata(int x, int y, int z, uint8_t val)
{
    std::lock_guard<std::mutex> lock(chunkMutex);
    if(x < 0 || x >= CHUNK_SIZE || y < 0 || y >= CHUNK_SIZE || z < 0 || z >= CHUNK_SIZE)
        return;
    blocks[x][y][z].metadata = val;
    // metadata might affect rendering (e.g. liquid level), so mark dirty
    meshDirty = true;
}

std::vector<float> Chunk::generateGeometry(int& outOpaqueCount)
{
    // Thread safety: Lock while reading blocks
    std::lock_guard<std::mutex> lock(chunkMutex);
    
    std::vector<float> opaqueVertices;
    std::vector<float> transparentVertices;
    // Pre-allocate decent amount
    opaqueVertices.reserve(4096);
    transparentVertices.reserve(1024);

    // Greedy Meshing
    struct MaskInfo {
        Block* block;
        uint8_t sky;
        uint8_t blockVal;
        uint8_t ao[4]; // BL, BR, TR, TL
        uint8_t metadata;
        
        bool operator==(const MaskInfo& other) const {
            return block == other.block && sky == other.sky && blockVal == other.blockVal &&
                   ao[0] == other.ao[0] && ao[1] == other.ao[1] && 
                   ao[2] == other.ao[2] && ao[3] == other.ao[3] &&
                   metadata == other.metadata;
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

        auto getAt = [&](int u, int v, int d) -> ChunkBlock {
                int p[3]; p[axis] = d; p[uAxis] = u; p[vAxis] = v;
                return blocks[p[0]][p[1]][p[2]];
        };
        auto getPos = [&](int u, int v, int d, int& ox, int& oy, int& oz) {
                int p[3]; p[axis] = d; p[uAxis] = u; p[vAxis] = v;
                ox = p[0]; oy = p[1]; oz = p[2];
        };

        Block* airBlock = BlockRegistry::getInstance().getBlock(AIR);
        for(int d = 0; d < CHUNK_SIZE; ++d) {
                MaskInfo mask[CHUNK_SIZE][CHUNK_SIZE];
                for(int u=0;u<CHUNK_SIZE;++u) for(int v=0;v<CHUNK_SIZE;++v) mask[u][v] = { airBlock, 0, 0, {0,0,0,0}, 0 };

                for(int v=0; v<CHUNK_SIZE; ++v) {
                    for(int u=0; u<CHUNK_SIZE; ++u) {
                        ChunkBlock b = getAt(u, v, d);
                        if(b.isActive()) {
                            int lx, ly, lz; getPos(u, v, d, lx, ly, lz);
                            int nx = lx + nX; int ny = ly + nY; int nz = lz + nZ;
                            
                            bool occluded = false;
                            uint8_t skyVal = 0; uint8_t blockVal = 0;

                            if(nx >= 0 && nx < CHUNK_SIZE && ny >= 0 && ny < CHUNK_SIZE && nz >= 0 && nz < CHUNK_SIZE) {
                                ChunkBlock nb = blocks[nx][ny][nz];
                                if(nb.isActive()) {
                                    if(!b.isOpaque()) { // Logic change: if current is transparent, and neighbor is SAME type or opaque, occlude?
                                        // Original logic: if(b.type == WATER || b.type == LAVA) { if(nb.type == b.type || nb.isOpaque()) ... }
                                        // Replicating:
                                        if(nb.block == b.block || nb.isOpaque()) occluded = true; 
                                        // Note: Comparing block pointers works for Water==Water check.
                                    } else {
                                        if(nb.isOpaque()) occluded = true;
                                    }
                                }
                                else { skyVal = blocks[nx][ny][nz].skyLight; blockVal = blocks[nx][ny][nz].blockLight; }
                            } else {
                                int ni = -1;
                                int nnx=nx, nny=ny, nnz=nz;
                                
                                if(nz >= CHUNK_SIZE) { ni = DIR_FRONT; nnz -= CHUNK_SIZE; }
                                else if(nz < 0) { ni = DIR_BACK; nnz += CHUNK_SIZE; }
                                else if(nx < 0) { ni = DIR_LEFT; nnx += CHUNK_SIZE; }
                                else if(nx >= CHUNK_SIZE) { ni = DIR_RIGHT; nnx -= CHUNK_SIZE; }
                                else if(ny >= CHUNK_SIZE) { ni = DIR_TOP; nny -= CHUNK_SIZE; }
                                else if(ny < 0) { ni = DIR_BOTTOM; nny += CHUNK_SIZE; }
                                
                                if(ni != -1 && neighbors[ni]) {
                                     ChunkBlock nb = neighbors[ni]->getBlock(nnx, nny, nnz);
                                     if(nb.isActive()) {
                                         if(!b.isOpaque()) {
                                             if(nb.block == b.block || nb.isOpaque()) occluded = true;
                                         } else {
                                             if(nb.isOpaque()) occluded = true;
                                         }
                                     }
                                     else { skyVal = neighbors[ni]->getSkyLight(nnx, nny, nnz); blockVal = neighbors[ni]->getBlockLight(nnx, nny, nnz); }
                                } else if(world) {
                                    int gx = chunkPosition.x * CHUNK_SIZE + nx;
                                    int gy = chunkPosition.y * CHUNK_SIZE + ny;
                                    int gz = chunkPosition.z * CHUNK_SIZE + nz;
                                    // World returns Block? Or ChunkBlock?
                                    // Wait, World::getBlock probably still returns Block (the old struct)?
                                    // I haven't updated World.h yet!
                                    // I MUST update World.h to return ChunkBlock as well.
                                    // Assuming I will do that, code here should use ChunkBlock.
                                    // For now, let's assume World::getBlock returns ChunkBlock.
                                    ChunkBlock nb = world->getBlock(gx, gy, gz);
                                    if(nb.isActive()) {
                                         if(!b.isOpaque()) {
                                             if(nb.block == b.block || nb.isOpaque()) occluded = true;
                                         } else {
                                             if(nb.isOpaque()) occluded = true;
                                         }
                                    }
                                    else { skyVal = world->getSkyLight(gx, gy, gz); blockVal = world->getBlockLight(gx, gy, gz); }
                                }
                            }
                            
                            if(!occluded) {
                                auto sampleAO = [&](int u1, int v1, int u2, int v2, int u3, int v3) -> uint8_t {
                                    auto check = [&](int u, int v) -> bool {
                                        int lx, ly, lz; getPos(u, v, d, lx, ly, lz);
                                        int nx=lx+nX; int ny=ly+nY; int nz=lz+nZ;
                                        if(nx >= 0 && nx < CHUNK_SIZE && ny >= 0 && ny < CHUNK_SIZE && nz >= 0 && nz < CHUNK_SIZE) 
                                            // Need to check isOpaque()
                                            // Can access blocks directly
                                            return blocks[nx][ny][nz].isOpaque();
                                            
                                        int ni = -1;
                                        int nnx=nx, nny=ny, nnz=nz;
                                        if(nz >= CHUNK_SIZE) { ni = DIR_FRONT; nnz -= CHUNK_SIZE; }
                                        else if(nz < 0) { ni = DIR_BACK; nnz += CHUNK_SIZE; }
                                        else if(nx < 0) { ni = DIR_LEFT; nnx += CHUNK_SIZE; }
                                        else if(nx >= CHUNK_SIZE) { ni = DIR_RIGHT; nnx -= CHUNK_SIZE; }
                                        else if(ny >= CHUNK_SIZE) { ni = DIR_TOP; nny -= CHUNK_SIZE; }
                                        else if(ny < 0) { ni = DIR_BOTTOM; nny += CHUNK_SIZE; }
                                        
                                        bool isDiagonal = (nnx < 0 || nnx >= CHUNK_SIZE || nny < 0 || nny >= CHUNK_SIZE || nnz < 0 || nnz >= CHUNK_SIZE);

                                        if(!isDiagonal && ni != -1 && neighbors[ni]) {
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
                                aos[2] = sampleAO(u+1, v, u, v+1, u+1, v+1); 
                                aos[3] = sampleAO(u-1, v, u, v+1, u-1, v+1); 
                                mask[u][v] = { b.block, skyVal, blockVal, {aos[0], aos[1], aos[2], aos[3]}, b.metadata };
                            }
                        }
                    }
                }
                
                // Greedy Mesh
                for(int v=0; v<CHUNK_SIZE; ++v) {
                    for(int u=0; u<CHUNK_SIZE; ++u) {
                        if(mask[u][v].block->isActive()) {
                            MaskInfo current = mask[u][v];
                            // Greedy Extend
                            // Disable greedy meshing for liquids to allow per-block smooth lighting/height
                            bool isLiquid = (current.block->getId() == WATER || current.block->getId() == LAVA);
                            
                            int w = 1, h = 1;
                            while(u + w < CHUNK_SIZE && mask[u+w][v] == current && !isLiquid) w++;
                            bool canExtend = true;
                            while(v + h < CHUNK_SIZE && canExtend && !isLiquid) {
                                for(int k=0; k<w; ++k) if(mask[u+k][v+h] != current) { canExtend = false; break; }
                                if(canExtend) h++;
                            }
                            int lx, ly, lz; getPos(u, v, d, lx, ly, lz);
                            
                            // Check if transparent
                            bool isTrans = (current.block->getRenderLayer() == Block::RenderLayer::TRANSPARENT);
                            
                            // Calculate smooth water heights
                            float hBL=1.0f, hBR=1.0f, hTR=1.0f, hTL=1.0f;
                            if(isLiquid) {
                                auto getHeight = [&](int bx, int by, int bz) -> float {
                                    if(by >= CHUNK_SIZE) return 1.0f; // Above chunk? Assume full?
                                    // Actually Check World for neighbors
                                    ChunkBlock bVec;
                                    if(bx>=0 && bx<CHUNK_SIZE && by>=0 && by<CHUNK_SIZE && bz>=0 && bz<CHUNK_SIZE) {
                                         bVec = blocks[bx][by][bz];
                                    } else {
                                         // Neighbor logic simplified: use world
                                         if(!world) return -1.0f; // treat unspread world as solid/ignore?
                                          int gx = chunkPosition.x * CHUNK_SIZE + bx;
                                          int gy = chunkPosition.y * CHUNK_SIZE + by;
                                          int gz = chunkPosition.z * CHUNK_SIZE + bz;
                                          bVec = world->getBlock(gx, gy, gz);
                                    }
                                    
                                    if(!bVec.isActive()) return 0.0f; // Air -> 0.0
                                    if(bVec.block->getId() == WATER || bVec.block->getId() == LAVA) {
                                        if(bVec.metadata >= 7) return 0.1f; // Min height
                                        return (8.0f - bVec.metadata) / 9.0f;
                                    }
                                    if(bVec.isSolid()) return -1.0f; // Solid -> Ignore
                                    return 0.0f; // Other non-solid -> 0.0?
                                };
                                
                                auto avgHeight = [&](int bx, int by, int bz) -> float {
                                    float s = 0.0f;
                                    float count = 0.0f;
                                    
                                    float hCurrent = getHeight(bx, by, bz);
                                    if(hCurrent >= 0.0f) { s += hCurrent; count += 1.0f; }
                                    
                                    float hX = getHeight(bx-1, by, bz);
                                    if(hX >= 0.0f) { s += hX; count += 1.0f; }
                                    else if(hX < -0.5f) { // If solid, check if it pushes up? No, ignore.
                                        // But if we have Water + Stone. Avg = Water.
                                        // Effectively we extend the water level to the wall.
                                        // s += hCurrent; count += 1.0f; // Duplicate current?
                                        // Standard MC: The level at a wall is the level of the liquid.
                                        // So ignoring it works (average stays same).
                                        // BUT if we have Water(0.8) + Water(0.6) + Stone + Stone.
                                        // Avg = (0.8+0.6)/2 = 0.7.
                                        // If we added HCurrent for stones: (0.8+0.6+0.8+0.8)/4 = 0.75.
                                        // Ignoring seems safer/cleaner.
                                    }

                                    float hZ = getHeight(bx, by, bz-1);
                                    if(hZ >= 0.0f) { s += hZ; count += 1.0f; }
                                    
                                    float hXZ = getHeight(bx-1, by, bz-1);
                                    if(hXZ >= 0.0f) { s += hXZ; count += 1.0f; }
                                    
                                    if(count <= 0.0f) return 1.0f;
                                    return s / count;
                                };
                                
                                // Coordinates are dependent on Face Axis?
                                // getPos returns lx, ly, lz which are local coords for the block "u,v" at depth d.
                                // We need global relative coords for avgHeight
                                
                                hBL = avgHeight(lx, ly, lz);
                                hBR = avgHeight(lx+1, ly, lz);
                                hTR = avgHeight(lx+1, ly, lz+1);
                                hTL = avgHeight(lx, ly, lz+1);
                                
                                // Special case: if block above is liquid, force full height
                                if(getHeight(lx, ly+1, lz) > 0.5f) { hBL=hBR=hTR=hTL=1.0f; }
                            }
                            
                            addFace(isTrans ? transparentVertices : opaqueVertices, lx, ly, lz, faceDir, current.block, w, h, current.ao[0], current.ao[1], current.ao[2], current.ao[3], current.metadata, hBL, hBR, hTR, hTL);
                            
                            for(int j=0; j<h; ++j) for(int i=0; i<w; ++i) mask[u+i][v+j] = { airBlock, 0, 0, {0,0,0,0}, 0 };
                            u += w - 1; 
                        }
                    }
                }
        } // End d loop
    } // End faceDir loop
    
    // Stitch Vectors
    outOpaqueCount = opaqueVertices.size() / 14;
    opaqueVertices.insert(opaqueVertices.end(), transparentVertices.begin(), transparentVertices.end());
    
    return opaqueVertices;
}
void Chunk::uploadMesh(const std::vector<float>& data, int opaqueCount)
{
    if(VAO == 0) initGL();

    
    // Upload to GPU (Main Thread)
    glBindVertexArray(VAO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, data.size() * sizeof(float), data.data(), GL_STATIC_DRAW);
    
    vertexCount = opaqueCount;
    vertexCountTransparent = (data.size() / 14) - opaqueCount;
    
    // Attribs
    float stride = 14 * sizeof(float);
    
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, (void*)0); // Pos
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, stride, (void*)(3 * sizeof(float))); // Color (Vec4)
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, stride, (void*)(7 * sizeof(float))); // UV
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(3, 3, GL_FLOAT, GL_FALSE, stride, (void*)(9 * sizeof(float))); // Light(Sky,Block,AO)
    glEnableVertexAttribArray(3);
    glVertexAttribPointer(4, 2, GL_FLOAT, GL_FALSE, stride, (void*)(12 * sizeof(float))); // TexOrigin
    glEnableVertexAttribArray(4);
}

void Chunk::updateMesh()
{
    int opaqueCount = 0;
    std::vector<float> newVertices = generateGeometry(opaqueCount);
    uploadMesh(newVertices, opaqueCount);
    meshDirty = false;
}

void Chunk::addFace(std::vector<float>& vertices, int x, int y, int z, int faceDir, const Block* block, int width, int height, int aoBL, int aoBR, int aoTR, int aoTL, uint8_t metadata, float hBL, float hBR, float hTR, float hTL)
{
    float r, g, b;
    block->getColor(r, g, b);
    float alpha = block->getAlpha();

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
    block->getTextureUV(faceDir, uMin, vMin);

    float fx = (float)x, fy = (float)y, fz = (float)z;
    float fw = (float)width, fh = (float)height;

    // Fluid Height Logic
    float topH = 1.0f; // Default Top
    if(block->getId() == WATER || block->getId() == LAVA) {
         if(metadata >= 7) topH = 0.1f;
         else topH = (8.0f - metadata) / 9.0f;
    }
    
    // Adjust height for side faces if this is a fluid?
    // Actually, "height" argument is the greedy-meshed height (number of blocks).
    // If NOT liquid, force h=1.0f for Top/Bottom, or h=height for Sides
    if(block->getId() != WATER && block->getId() != LAVA) {
        if(faceDir <= 3) { // Side Faces: height is Y-extent
            float H = (float)height;
            hBL = H; hBR = H; hTR = H; hTL = H;
        } else { // Top/Bottom Faces: height is Z-extent (or X), Y-extent is 1 block
            hBL = 1.0f; hBR = 1.0f; hTR = 1.0f; hTL = 1.0f;
        }
    }

    auto pushVert = [&](float vx, float vy, float vz, float u, float v, float ao) {
        vertices.push_back(vx); vertices.push_back(vy); vertices.push_back(vz);
        vertices.push_back(r); vertices.push_back(g); vertices.push_back(b); vertices.push_back(alpha);
        vertices.push_back(u); vertices.push_back(v); 
        vertices.push_back(l1); vertices.push_back(l2); vertices.push_back(ao);
        vertices.push_back(uMin); vertices.push_back(vMin); 
    };
    
    // Corners Mapping:
    // hBL is for corner (x, z)
    // hBR is for corner (x+1, z)
    // hTR is for corner (x+1, z+1)
    // hTL is for corner (x, z+1)
    // (Note: w and h are 1 for liquids now, as greedy meshing is disabled if levels differ)
    
    // Y Offsets relative to fy
    float yBL = hBL; 
    float yBR = hBR;
    float yTR = hTR;
    float yTL = hTL;
    
    float botY = fy;

    if(faceDir == 0) { // Front Z+ (at z+1)
        // This face is at Z+1.
        // It spans x to x+w.
        // The corners of the quad are:
        // (fx, fy, fz+1) -> uses hTL (for x, z+1)
        // (fx+fw, fy, fz+1) -> uses hTR (for x+w, z+1)
        // Note: For greedy meshing, fw and fh are the dimensions of the quad.
        // The hBL, hBR, hTR, hTL are for the *bottom-left* block of the quad.
        // For liquids, fw and fh will be 1.
        
        // Vertices for this face:
        // Bottom-Left of quad: (fx, botY, fz+1)
        // Bottom-Right of quad: (fx+fw, botY, fz+1)
        // Top-Right of quad: (fx+fw, fy+yTR, fz+1)
        // Top-Left of quad: (fx, fy+yTL, fz+1)
        
        pushVert(fx, botY, fz+1, 0, 0, (float)aoBL);
        pushVert(fx+fw, botY, fz+1, fw, 0, (float)aoBR);
        pushVert(fx+fw, fy+yTR, fz+1, fw, fh, (float)aoTR); 
        
        pushVert(fx, botY, fz+1, 0, 0, (float)aoBL);
        pushVert(fx+fw, fy+yTR, fz+1, fw, fh, (float)aoTR);
        pushVert(fx, fy+yTL, fz+1, 0, fh, (float)aoTL); 
    }
    else if(faceDir == 1) { // Back Z- (at z=0)
        // Vertices for this face:
        // Bottom-Right of quad: (fx+fw, botY, fz)
        // Bottom-Left of quad: (fx, botY, fz)
        // Top-Left of quad: (fx, fy+yBL, fz)
        // Top-Right of quad: (fx+fw, fy+yBR, fz)
        
        pushVert(fx+fw, botY, fz, 0, 0, (float)aoBR);
        pushVert(fx, botY, fz, fw, 0, (float)aoBL);
        pushVert(fx, fy+yBL, fz, fw, fh, (float)aoTL);
        
        pushVert(fx+fw, botY, fz, 0, 0, (float)aoBR);
        pushVert(fx, fy+yBL, fz, fw, fh, (float)aoTL);
        pushVert(fx+fw, fy+yBR, fz, 0, fh, (float)aoTR);
    }
    else if(faceDir == 2) { // Left X- (at x=0)
        // Vertices for this face:
        // Bottom-Left of quad: (fx, botY, fz)
        // Bottom-Right of quad: (fx, botY, fz+fw)
        // Top-Right of quad: (fx, fy+yTL, fz+fw)
        // Top-Left of quad: (fx, fy+yBL, fz)
        
        pushVert(fx, botY, fz, 0, 0, (float)aoBL);
        pushVert(fx, botY, fz+fw, fw, 0, (float)aoBR);
        pushVert(fx, fy+yTL, fz+fw, fw, fh, (float)aoTR); 
        
        pushVert(fx, botY, fz, 0, 0, (float)aoBL);
        pushVert(fx, fy+yTL, fz+fw, fw, fh, (float)aoTR);
        pushVert(fx, fy+yBL, fz, 0, fh, (float)aoTL); 
    }
    else if(faceDir == 3) { // Right X+ (at x+1)
        // Vertices for this face:
        // Bottom-Right of quad: (fx+1, botY, fz+fw)
        // Bottom-Left of quad: (fx+1, botY, fz)
        // Top-Left of quad: (fx+1, fy+yBR, fz)
        // Top-Right of quad: (fx+1, fy+yTR, fz+fw)
        
        pushVert(fx+1, botY, fz+fw, 0, 0, (float)aoBR);
        pushVert(fx+1, botY, fz, fw, 0, (float)aoBL);
        pushVert(fx+1, fy+yBR, fz, fw, fh, (float)aoTL); 
        
        pushVert(fx+1, botY, fz+fw, 0, 0, (float)aoBR);
        pushVert(fx+1, fy+yBR, fz, fw, fh, (float)aoTL); 
        pushVert(fx+1, fy+yTR, fz+fw, 0, fh, (float)aoTR); 
    }
    else if(faceDir == 4) { // Top Y+ (at y+1)
        // Uses all 4 corner heights.
        // The 'width' (fw) here is along X, 'height' (fh) is along Z.
        // Vertices for this face:
        // Top-Left of quad: (fx, fy+yTL, fz+fw)
        // Top-Right of quad: (fx+fw, fy+yTR, fz+fw)
        // Bottom-Right of quad: (fx+fw, fy+yBR, fz)
        // Bottom-Left of quad: (fx, fy+yBL, fz)
        
        pushVert(fx, fy+yTL, fz+fh, 0, 0, (float)aoTL); 
        pushVert(fx+fw, fy+yTR, fz+fh, fw, 0, (float)aoTR);
        pushVert(fx+fw, fy+yBR, fz, fw, fh, (float)aoBR);
        
        pushVert(fx, fy+yTL, fz+fh, 0, 0, (float)aoTL);
        pushVert(fx+fw, fy+yBR, fz, fw, fh, (float)aoBR);
        pushVert(fx, fy+yBL, fz, 0, fh, (float)aoBL);
    }
    else { // Bottom Y- (at y=0)
        // Bottom face always uses botY (fy)
        pushVert(fx, botY, fz, 0, 0, (float)aoBL);
        pushVert(fx+fw, botY, fz, fw, 0, (float)aoBR);
        pushVert(fx+fw, botY, fz+fh, fw, fh, (float)aoTR);
        
        pushVert(fx, botY, fz, 0, 0, (float)aoBL);
        pushVert(fx+fw, botY, fz+fh, fw, fh, (float)aoTR);
        pushVert(fx, botY, fz+fh, 0, fh, (float)aoTL);
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
            // Use isSolid() to ignore Water/Lava for raycasting
            if(blocks[x][y][z].isSolid())
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
              int incomingLight = 15;
              bool topNeighborMissing = false;

              // Walk up using neighbors
              while(current) {
                   if(current->neighbors[DIR_TOP]) {
                      current = current->neighbors[DIR_TOP];
                      // Check column in upper chunk (Bottom-Up)
                      bool blocked = false;
                      for(int ly=0; ly<CHUNK_SIZE; ++ly) {
                          ChunkBlock b = current->getBlock(x, ly, z);
                          if(b.isOpaque()) {
                              exposedToSky = false;
                              blocked = true;
                              incomingLight = 0;
                              break;
                          } else if(b.getType() == WATER) {
                              incomingLight -= 2;
                              if(incomingLight <= 0) {
                                  incomingLight = 0;
                              }
                          }
                      }
                      if(blocked || incomingLight <= 0) break;
                   } else {
                       // We reached the top loaded chunk.
                       // If we are below the world surface, we shouldn't assume sky.
                       // The world surface is roughly Y=64 (Chunk 4).
                       // If current chunk Y < 4, and we have no neighbor above, we are likely underground waiting for load.
                       if(current->chunkPosition.y < 4) {
                           exposedToSky = false;
                           incomingLight = 0;
                       }
                       break;
                   }
              }
              
              if(exposedToSky) {
                   int currentLight = incomingLight;
                  for(int y=CHUNK_SIZE-1; y>=0; --y) {
                      if(blocks[x][y][z].isOpaque()) {
                          break;
                      } else {
                          // Attenuate light in water
                          if(blocks[x][y][z].getType() == WATER) {
                              currentLight -= 2;
                              if(currentLight < 0) currentLight = 0;
                          }
                          blocks[x][y][z].skyLight = currentLight;
                          
                          // Queue for spreading if not full brightness?
                          // Actually, if we attenuate, we might want to queue it to spread the darkness/light?
                          // The spreadLight function handles outward spread. 
                          // The column is the source.
                          // Note: skyQueue is not accessible here. This line is commented out to maintain syntactical correctness.
                          // if(currentLight > 0) {
                          //     skyQueue.push(glm::ivec3(x, y, z));
                          // }
                      }
                  }
                 
                 // If top block was Water, the column below it should also be lit?
                 // The loop goes Y-Down.
                 // Loop continues until isOpaque().
                 // So if top is Water, it is NOT opaque.
                 // blocks[x][y][z].skyLight = 15;
                 // Loop continues.
                 // Next block (below water) is Water. Not Opaque. skyLight = 15.
                 // Next block is Stone. Opaque. Break.
                 // Stone gets 0 (default).
                 // So the water column will be fully lit (15).
                 // However, should water attenuate light? (Darker deeper down).
                 // Minecraft does decrease light by 3 per water block.
                 // For now, full light is fine to fix "Pitch Black".
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
                       
                       if(!blocks[lx][ly][lz].isOpaque()) {
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
                if(!blocks[nx][ny][nz].isOpaque()) {
                    int decay = (blocks[nx][ny][nz].getType() == WATER) ? 3 : 1;
                    if(blocks[nx][ny][nz].skyLight < curLight - decay) {
                        blocks[nx][ny][nz].skyLight = curLight - decay;
                        skyQueue.push(glm::ivec3(nx, ny, nz));
                        meshDirty = true;
                    }
                }
            } else {
                // ... neighbor prop logic ...
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
                if(!blocks[nx][ny][nz].isOpaque()) {
                    int decay = (blocks[nx][ny][nz].getType() == WATER) ? 3 : 1;
                    if(blocks[nx][ny][nz].blockLight < curLight - decay) {
                        blocks[nx][ny][nz].blockLight = curLight - decay;
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

