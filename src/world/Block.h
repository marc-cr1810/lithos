#ifndef BLOCK_H
#define BLOCK_H

#include <cstdint>

enum BlockType : uint8_t {
    AIR = 0,
    DIRT = 1,
    STONE = 2,
    GRASS = 3,
    WOOD = 4,
    LEAVES = 5
};

struct Block {
    uint8_t type;
    uint8_t light = 0; // 0-15
    
    bool isActive() const {
        return type != AIR;
    }
};

#endif
