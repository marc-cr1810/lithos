#ifndef BLOCK_H
#define BLOCK_H

#include <cstdint>

enum BlockType {
    AIR = 0,
    DIRT = 1,
    GRASS = 2,
    STONE = 3,
    WOOD = 4,
    LEAVES = 5,
    COAL_ORE = 6,
    IRON_ORE = 7,
    GLOWSTONE = 8
};

struct Block {
    uint8_t type;
    uint8_t skyLight = 0;   // 0-15 Sun
    uint8_t blockLight = 0; // 0-15 Torches
    
    bool isActive() const {
        return type != AIR;
    }

    bool isOpaque() const {
        return type != AIR && type != LEAVES && type != GLOWSTONE;
    }
    
    uint8_t getEmission() const {
        if(type == GLOWSTONE) return 15;
        return 0;
    }
};

#endif
