#ifndef FLORA_DECORATOR_H
#define FLORA_DECORATOR_H

#include "WorldDecorator.h"

class FloraDecorator : public WorldDecorator {
public:
  void Decorate(Chunk &chunk, WorldGenerator &generator) override;
};

#endif
