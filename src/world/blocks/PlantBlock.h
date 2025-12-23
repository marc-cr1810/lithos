#ifndef PLANT_BLOCK_H
#define PLANT_BLOCK_H

#include "../Block.h"

class PlantBlock : public Block {
public:
    PlantBlock(uint8_t id, const std::string& name) : Block(id, name) {}
    bool isOpaque() const override { return false; }
    RenderLayer getRenderLayer() const override { return RenderLayer::CUTOUT; }
    
    void getTextureUV(int faceDir, float& uMin, float& vMin) const override {
        if(id == BlockType::LEAVES) { uMin = 0.50f; vMin = 0.25f; }
    }
    
    void getColor(float& r, float& g, float& b) const override {
         if(id == BlockType::LEAVES) { r = 0.2f; g = 0.8f; b = 0.2f; }
         else { r = 1.0f; g = 1.0f; b = 1.0f; }
    }
};

#endif
