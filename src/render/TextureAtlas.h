#ifndef TEXTURE_ATLAS_H
#define TEXTURE_ATLAS_H

#include <vector>

class TextureAtlas
{
public:
    TextureAtlas(int width, int height, int slotSize);
    ~TextureAtlas();

    // Generates the procedural textures
    void Generate();
    
    // Access raw data (for glTexImage2D)
    unsigned char* GetData() { return data.data(); }
    int GetWidth() const { return width; }
    int GetHeight() const { return height; }

private:
    int width;
    int height;
    int slotSize;
    std::vector<unsigned char> data;
    
    void GenerateStone(int slotX, int slotY);
    void GenerateDirt(int slotX, int slotY);
    void GenerateGrassTop(int slotX, int slotY);
    void GenerateWoodSide(int slotX, int slotY);
    void GenerateWoodTop(int slotX, int slotY);
    void GenerateLeaves(int slotX, int slotY);
    void GenerateOre(int slotX, int slotY, int r, int g, int b);
    void GenerateGlowstone(int slotX, int slotY);
    
    void SetPixel(int x, int y, unsigned char r, unsigned char g, unsigned char b);
};

#endif
