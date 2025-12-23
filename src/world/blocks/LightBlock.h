#ifndef LIGHT_BLOCK_H
#define LIGHT_BLOCK_H

#include "../Block.h"

class LightBlock : public Block {
    uint8_t emissionLevel;
public:
    LightBlock(uint8_t id, const std::string& name, uint8_t emission) 
        : Block(id, name), emissionLevel(emission) {}
    uint8_t getEmission() const override { return emissionLevel; }
    bool isOpaque() const override { return false; } 
    
    void getTextureUV(int faceDir, float& uMin, float& vMin) const override {
        if(id == BlockType::GLOWSTONE) { uMin = 0.00f; vMin = 0.50f; }
    }
};

#endif
