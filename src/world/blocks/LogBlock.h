#ifndef LOG_BLOCK_H
#define LOG_BLOCK_H

#include "../Block.h"
#include "SolidBlock.h"

class LogBlock : public SolidBlock {
public:
  LogBlock(uint8_t id, const std::string &name);
  void getTextureUV(int faceDir, float &u, float &v, int x, int y, int z,
                    uint8_t metadata, int layer = 0) const override;
};

#endif
