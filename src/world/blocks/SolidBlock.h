#ifndef SOLID_BLOCK_H
#define SOLID_BLOCK_H

#include "../Block.h"

class SolidBlock : public Block {
public:
    SolidBlock(uint8_t id, const std::string& name) : Block(id, name) {}
    
    void getTextureUV(int faceDir, float& uMin, float& vMin) const override {
        if(id == BlockType::DIRT) { uMin = 0.25f; vMin = 0.00f; }
        else if(id == BlockType::GRASS) { uMin = 0.50f; vMin = 0.00f; }
        else if(id == BlockType::WOOD) {
            if(faceDir == 4 || faceDir == 5) { uMin = 0.25f; vMin = 0.25f; } 
            else { uMin = 0.00f; vMin = 0.25f; } 
        }
        else if(id == BlockType::COAL_ORE) { uMin = 0.75f; vMin = 0.00f; }
        else if(id == BlockType::IRON_ORE) { uMin = 0.75f; vMin = 0.25f; }
        else if(id == BlockType::SAND) { uMin = 0.75f; vMin = 0.50f; }
        else if(id == BlockType::GRAVEL) { uMin = 0.00f; vMin = 0.75f; }
        else { uMin = 0.00f; vMin = 0.00f; } 
    }

    void getColor(float& r, float& g, float& b) const override {
        if(id == BlockType::GRASS) { r = 0.0f; g = 1.0f; b = 0.0f; }
        else if(id == BlockType::DIRT) { r = 0.6f; g = 0.4f; b = 0.2f; }
        else { r = 1.0f; g = 1.0f; b = 1.0f; } 
    }
};

#endif
