#ifndef TEXTURE_H
#define TEXTURE_H

#include <GL/glew.h>
#include <string>

class Texture {
public:
  unsigned int ID;
  int Width, Height, nrChannels;

  Texture(const char *path);
  Texture(int width, int height, const unsigned char *data, int channels = 3);
  ~Texture();

  void bind();
};

#endif
