#include "TextureAtlas.h"
#include <cstdlib>
#include <cmath>
#include <algorithm>

TextureAtlas::TextureAtlas(int width, int height, int slotSize)
    : width(width), height(height), slotSize(slotSize)
{
    data.resize(width * height * 3);
}

TextureAtlas::~TextureAtlas()
{
}

void TextureAtlas::Generate()
{
    // Slot Map (4x4 grid of 16px)
    // 0,0: Stone
    // 1,0: Dirt
    // 2,0: Grass
    // 0,1: Wood Side
    // 1,1: Wood Top
    // 2,1: Leaves
    
    GenerateStone(0, 0);
    GenerateDirt(1, 0);
    GenerateGrassTop(2, 0);
    GenerateWoodSide(0, 1);
    GenerateWoodTop(1, 1);
    GenerateWoodTop(1, 1);
    GenerateLeaves(2, 1);
    
    // Ores
    // Coal (3,0) - Black spots
    GenerateOre(3, 0, 20, 20, 20); 
    // Iron (3,1) - Tan spots
    GenerateOre(3, 1, 210, 180, 140);
    
    // Glowstone (0, 2)
    // Glowstone (0, 2)
    GenerateGlowstone(0, 2);
    
    // Water (1, 2) - Blue Noise
    GenerateWater(1, 2);
    
    // Lava (2, 2) - Orange/Red Noise
    GenerateLava(2, 2);
}

void TextureAtlas::SetPixel(int x, int y, unsigned char r, unsigned char g, unsigned char b)
{
    if (x < 0 || x >= width || y < 0 || y >= height) return;
    int idx = (y * width + x) * 3;
    data[idx] = r;
    data[idx+1] = g;
    data[idx+2] = b;
}

void TextureAtlas::GenerateStone(int slotX, int slotY)
{
    int startX = slotX * slotSize;
    int startY = slotY * slotSize;
    
    for(int y=0; y<slotSize; ++y) {
        for(int x=0; x<slotSize; ++x) {
            // Stone: Rough noise (Darker Grey)
            int noise = rand() % 40 + 60; // 60-100
            
            // Add some "cracks" (darker spots)
            if(rand() % 20 == 0) noise -= 30;
            
            SetPixel(startX + x, startY + y, (unsigned char)noise, (unsigned char)noise, (unsigned char)noise);
        }
    }
}

void TextureAtlas::GenerateDirt(int slotX, int slotY)
{
    int startX = slotX * slotSize;
    int startY = slotY * slotSize;
    
    for(int y=0; y<slotSize; ++y) {
        for(int x=0; x<slotSize; ++x) {
            // Dirt: Smoother, speckled (Brownish Grey)
            // We want it distinct from Stone.
            // Dithered pattern checkboard?
            int noise = rand() % 40 + 100;
            
            // Speckles
            if((x + y) % 2 == 0) noise += 20;

            // Reduce contrast compared to stone
            unsigned char val = (unsigned char)noise;
            SetPixel(startX + x, startY + y, val, val, val);
        }
    }
}

void TextureAtlas::GenerateGrassTop(int slotX, int slotY)
{
    int startX = slotX * slotSize;
    int startY = slotY * slotSize;
    
    for(int y=0; y<slotSize; ++y) {
        for(int x=0; x<slotSize; ++x) {
            // Grass: Noise with some "blades" (lighter streaks)
            int noise = rand() % 50 + 150;
            
            // Blade check?
            if(rand() % 5 == 0) noise += 30;
            
            unsigned char val = (unsigned char)noise;
            SetPixel(startX + x, startY + y, val, val, val);
        }
    }
}

void TextureAtlas::GenerateWoodSide(int slotX, int slotY)
{
    int startX = slotX * slotSize;
    int startY = slotY * slotSize;
    
    for(int y=0; y<slotSize; ++y) {
        for(int x=0; x<slotSize; ++x) {
            // Wood Side: Chaotic Bark
            int noise = rand() % 40;
            // Wavy vertical stripes
            int shift = (y / 3); 
            bool fissure = ((x + shift) % 4 == 0);
            int fVal = fissure ? 30 : 0;
            if(rand()%10 > 7) fVal = 0; // Break fissures
            
            int val = 120 + noise - fVal;
            // Keep Vertex Color = White, so bake Color here?
            // User complained about "planks".
            // Let's use the Brown color here.
            unsigned char r = val; 
            unsigned char g = (unsigned char)(val * 0.7f); 
            unsigned char b = (unsigned char)(val * 0.5f);
            SetPixel(startX + x, startY + y, r, g, b);
        }
    }
}

void TextureAtlas::GenerateWoodTop(int slotX, int slotY)
{
    int startX = slotX * slotSize;
    int startY = slotY * slotSize;
    float center = slotSize / 2.0f - 0.5f;

    for(int y=0; y<slotSize; ++y) {
        for(int x=0; x<slotSize; ++x) {
            // Rings
            float dx = (float)x - center;
            float dy = (float)y - center;
            float dist = sqrt(dx*dx + dy*dy);
            
            int ring = (int)(dist * 1.5f) % 2; 
            int val = 140 + (ring * 40) + (rand()%20);
            
            unsigned char r = val; 
            unsigned char g = (unsigned char)(val * 0.8f); 
            unsigned char b = (unsigned char)(val * 0.6f);
            SetPixel(startX + x, startY + y, r, g, b);
        }
    }
}

void TextureAtlas::GenerateLeaves(int slotX, int slotY)
{
    int startX = slotX * slotSize;
    int startY = slotY * slotSize;
    
    for(int y=0; y<slotSize; ++y) {
        for(int x=0; x<slotSize; ++x) {
            // Leaves: Grid pattern + noise
            int noise = rand() % 60;
            if(x % 3 == 0 || y % 3 == 0) noise -= 20; 
            int val = 100 + noise;
            
            // Greyscale (Tinted Green by Vertex Color)
            SetPixel(startX + x, startY + y, (unsigned char)val, (unsigned char)val, (unsigned char)val);
        }
    }
}

void TextureAtlas::GenerateOre(int slotX, int slotY, int r, int g, int b)
{
    int startX = slotX * slotSize;
    int startY = slotY * slotSize;
    
    for(int y=0; y<slotSize; ++y) {
        for(int x=0; x<slotSize; ++x) {
            // Base Stone
            int noise = rand() % 60 + 80;
            if(rand() % 20 == 0) noise -= 30;
            
            unsigned char cr = (unsigned char)noise;
            unsigned char cg = (unsigned char)noise;
            unsigned char cb = (unsigned char)noise;
            
            // Ore spots
            // Simple noise check for spots
            if(rand() % 10 < 2) { 
                cr = (unsigned char)r;
                cg = (unsigned char)g;
                cb = (unsigned char)b;
            }
            
            SetPixel(startX + x, startY + y, cr, cg, cb);
        }
    }
}

void TextureAtlas::GenerateGlowstone(int slotX, int slotY)
{
    int startX = slotX * slotSize;
    int startY = slotY * slotSize;
    
    for(int y=0; y<slotSize; ++y) {
        for(int x=0; x<slotSize; ++x) {
            // Glowstone: Crystalline High contrast
            // Core: Bright Yellow/White
            // Crystal edges: Orange/Brown
            
            // Voronoi-ish or just simple blocky noise?
            int noise = rand() % 50;
            if(rand() % 5 == 0) noise -= 30; // Dark spots
            
            int baseVal = 200 + noise;
            if(baseVal > 255) baseVal = 255;
            if(baseVal < 0) baseVal = 0;
            
            unsigned char r = (unsigned char)baseVal; 
            unsigned char g = (unsigned char)(baseVal * 0.8f); 
            unsigned char b = (unsigned char)(baseVal * 0.4f);
            
            // Randomly very bright pixel (center of crystal)
            if(rand() % 20 == 0) {
                 r = 255; g = 255; b = 200;
            }
            
            SetPixel(startX + x, startY + y, r, g, b);
        }
    }
}

void TextureAtlas::GenerateWater(int slotX, int slotY)
{
    int startX = slotX * slotSize;
    int startY = slotY * slotSize;
    
    for(int y=0; y<slotSize; ++y) {
        for(int x=0; x<slotSize; ++x) {
            // Water: Blue noise
            int noise = rand() % 40 + 100;
            
            // Wavy pattern?
            if((x + y) % 4 == 0) noise += 20;

            unsigned char r = 40; 
            unsigned char g = 80; 
            unsigned char b = (unsigned char)noise + 50;
            
            SetPixel(startX + x, startY + y, r, g, b);
        }
    }
}

void TextureAtlas::GenerateLava(int slotX, int slotY)
{
    int startX = slotX * slotSize;
    int startY = slotY * slotSize;
    
    for(int y=0; y<slotSize; ++y) {
        for(int x=0; x<slotSize; ++x) {
            // Lava: Bright Orange/Red with dark clumps
            int noise = rand() % 60 + 150;
            
            unsigned char r = (unsigned char)noise; 
            unsigned char g = (unsigned char)(noise * 0.5f); 
            unsigned char b = 0;
            
            // Dark spots (crust)
            if(rand() % 10 == 0) {
                 r = 80; g = 20; b = 0;
            }
            
            SetPixel(startX + x, startY + y, r, g, b);
        }
    }
}
