#ifndef LOG_BLOCK_H
#define LOG_BLOCK_H

#include "../Block.h"
#include "SolidBlock.h"

class LogBlock : public SolidBlock {
public:
  LogBlock(block_id id, const std::string &name);
  void getTextureUV(int faceDir, float &u, float &v, int x, int y, int z,
                    uint8_t metadata, int layer = 0) const override;
  bool isLog() const override { return true; }
};

#endif
